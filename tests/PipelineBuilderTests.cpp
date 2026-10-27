// PipelineBuilder ve Pipeline testleri: bilesim, sira bagimsizligi, yasam dongusu.
#include "TestHarness.h"
#include "TestSupport.h"
#include "../ParserStage.h"
#include "../ErrorHandling.h"
#include "../PipelineBuilder.h"
#include "../PredicateFilterStage.h"
#include "../ProjectionStage.h"

typedef ListSource<LogRecord> Records;
typedef std::vector<std::string> Events;

static Records sampleRecords() {
    Records src;
    src.items.push_back(makeRecord(1, "1.1.1.1", "/api/a", 500));
    src.items.push_back(makeRecord(2, "1.1.1.2", "/index", 200));
    src.items.push_back(makeRecord(3, "1.1.1.3", "/api/b", 200));
    src.items.push_back(makeRecord(4, "1.1.1.4", "/img/x", 404));
    src.items.push_back(makeRecord(5, "1.1.1.5", "/api/c", 404));
    return src;
}

// Tip degistiren zincir: string -> LogRecord -> LogRecord
static void chainChangesTypeAndFilters() {
    ListSource<std::string> lines;
    lines.items.push_back("1.1.1.1 - - [01/Jan/2024:10:00:00 +0000] \"GET /a HTTP/1.1\" 200 1");
    lines.items.push_back("1.1.1.1 - - [01/Jan/2024:10:00:01 +0000] \"GET /b HTTP/1.1\" 500 1");
    lines.items.push_back("bozuk");

    ParserStage parser;
    RunContext context;
    PolicyStage<std::string,LogRecord> guardedParser(parser,context,"parse",ErrorPolicy(ErrorPolicy::SKIP));
    PredicateFilterStage errors(Predicates::statusAtLeast(400));
    CollectingSink<LogRecord> sink;

    Pipeline p = PipelineBuilder::from(lines).via(guardedParser).via(errors).to(sink);
    p.run();

    CHECK(sink.items.size() == 1 && sink.items[0].path() == "/b");
    CHECK(parser.badCount() == 1);
}

// Ayni asamalar, dogrudan kaynak -> sink (sifir asama) da calisir.
static void zeroStagesWorks() {
    Records src = sampleRecords();
    CollectingSink<LogRecord> sink;
    PipelineBuilder::from(src).to(sink).run();
    CHECK(sink.items.size() == 5);
}

// Ayni asama turunden birden fazlasi, herhangi bir sirada eklenebilir.
static void sameStageTypeUsedTwice() {
    Records src = sampleRecords();
    PredicateFilterStage errors(Predicates::statusAtLeast(400));
    PredicateFilterStage api(Predicates::pathStartsWith("/api"));
    CollectingSink<LogRecord> sink;

    PipelineBuilder::from(src).via(errors).via(api).to(sink).run();

    CHECK(sink.items.size() == 2);
    CHECK(sink.items[0].path() == "/api/a" && sink.items[1].path() == "/api/c");
}

// Filtre sirasi degisince SONUC ayni kalir; yalnizca is miktari degisir.
static void filterOrderDoesNotChangeResult() {
    Records src = sampleRecords();

    PredicateFilterStage errors1(Predicates::statusAtLeast(400));
    PredicateFilterStage api1(Predicates::pathStartsWith("/api"));
    CollectingSink<LogRecord> sinkA;
    PipelineBuilder::from(src).via(errors1).via(api1).to(sinkA).run();

    PredicateFilterStage errors2(Predicates::statusAtLeast(400));
    PredicateFilterStage api2(Predicates::pathStartsWith("/api"));
    CollectingSink<LogRecord> sinkB;
    PipelineBuilder::from(src).via(api2).via(errors2).to(sinkB).run();

    CHECK(sinkA.items.size() == sinkB.items.size());
    for (size_t i = 0; i < sinkA.items.size() && i < sinkB.items.size(); ++i)
        CHECK(sinkA.items[i].raw() == sinkB.items[i].raw());

    // Ilk sira: durum filtresi 5 kayit gorur, 3'u gecer -> yol filtresi 3 kayit gorur.
    CHECK(errors1.evaluatedCount() == 5 && api1.evaluatedCount() == 3);
    // Ters sira: yol filtresi 5 gorur, 3'u gecer -> durum filtresi 3 gorur.
    CHECK(api2.evaluatedCount() == 5 && errors2.evaluatedCount() == 3);
}

static void filterThenProjectionEqualsProjectionThenFilter() {
    Records src = sampleRecords();
    std::vector<std::string> fields;
    fields.push_back("path");
    fields.push_back("status");                              // filtrenin ihtiyaci olan alan korunur

    ProjectionStage proj1(fields);
    PredicateFilterStage filt1(Predicates::statusAtLeast(400));
    CollectingSink<LogRecord> sinkA;
    PipelineBuilder::from(src).via(filt1).via(proj1).to(sinkA).run();

    ProjectionStage proj2(fields);
    PredicateFilterStage filt2(Predicates::statusAtLeast(400));
    CollectingSink<LogRecord> sinkB;
    PipelineBuilder::from(src).via(proj2).via(filt2).to(sinkB).run();

    RecordFormatter fmt = LogFields::tabSeparated(fields);
    CHECK(sinkA.items.size() == 3 && sinkB.items.size() == 3);
    for (size_t i = 0; i < sinkA.items.size() && i < sinkB.items.size(); ++i)
        CHECK(fmt(sinkA.items[i]) == fmt(sinkB.items[i]));

    CHECK(proj1.projectedCount() == 3);                      // filtre once: 3 kayit projekte edilir
    CHECK(proj2.projectedCount() == 5);                      // projeksiyon once: 5 kayit
}

static void lifecycleOrder() {
    Events log;
    Records src = sampleRecords();
    TraceStage<LogRecord> a("a", log), b("b", log);
    TraceSink<LogRecord> sink(log);

    PipelineBuilder::from(src).via(a).via(b).to(sink).run();

    // open: sink -> b -> a ; close: a -> b -> sink
    const char* expected[] = {"open:sink", "open:b", "open:a", "close:a", "close:b", "close:sink"};
    CHECK(log.size() == 6);
    for (size_t i = 0; i < log.size() && i < 6; ++i) CHECK(log[i] == expected[i]);
}

static void failedOpenClosesWhatWasOpened() {
    Events log;
    Records src = sampleRecords();
    TraceStage<LogRecord> a("a", log), b("b", log, true);    // b acilirken hata verir
    TraceSink<LogRecord> sink(log);
    Pipeline p = PipelineBuilder::from(src).via(a).via(b).to(sink);

    CHECK_THROWS(p.run(), StageException);

    // sink acildi, b acilamadi, a'ya hic sira gelmedi -> yalnizca sink kapatilir.
    CHECK(log.size() == 2 && log[0] == "open:sink" && log[1] == "close:sink");
}

static void errorWhileProcessingStillCloses() {
    Events log;
    Records src = sampleRecords();
    TraceStage<LogRecord> a("a", log);
    TraceSink<LogRecord> sink(log, true);                    // consume() hata verir
    Pipeline p = PipelineBuilder::from(src).via(a).to(sink);

    CHECK_THROWS(p.run(), StageException);

    CHECK(log.size() == 4);
    CHECK(log[2] == "close:a" && log[3] == "close:sink");    // hataya ragmen kapatildi
}

int main() {
    run("tip degistiren zincir (string -> LogRecord) + filtre", chainChangesTypeAndFilters);
    run("sifir asama: kaynak -> sink", zeroStagesWorks);
    run("ayni asama turu iki kez eklenebilir", sameStageTypeUsedTwice);
    run("filtre sirasi sonucu degistirmez, isi degistirir", filterOrderDoesNotChangeResult);
    run("filtre/projeksiyon sirasi: ayni sonuc, farkli is", filterThenProjectionEqualsProjectionThenFilter);
    run("yasam dongusu sirasi", lifecycleOrder);
    run("basarisiz open(): acilanlar kapatilir", failedOpenClosesWhatWasOpened);
    run("akista hata: close() yine cagrilir", errorWhileProcessingStillCloses);
    return finish();
}
