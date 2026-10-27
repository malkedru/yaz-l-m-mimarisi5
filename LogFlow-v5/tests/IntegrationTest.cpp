// Entegrasyon testi: kucuk bir fixture dosyasini gercek hattan gecirir ve
// cikti DOSYASININ icerigini dogrular.
//
//   FileLineSource -> ParserStage -> PredicateFilterStage -> ProjectionStage -> FileSink
//
// Calistirma (proje kokunden):  ./build/IntegrationTest [fixture_yolu]
#include <cstdio>
#include "TestHarness.h"
#include "TestSupport.h"
#include "../FileLineSource.h"
#include "../FileSink.h"
#include "../ParserStage.h"
#include "../ErrorHandling.h"
#include "../PipelineBuilder.h"
#include "../PredicateFilterStage.h"
#include "../ProjectionStage.h"

static std::string fixturePath = "tests/fixtures/mini.log";
static const char* OUT = "integration_out.tmp";

static std::vector<std::string> projectedFields() {
    std::vector<std::string> f;
    f.push_back("timestamp"); f.push_back("clientIp"); f.push_back("status"); f.push_back("path");
    return f;
}

// 3 asamali hat: ayristir -> filtrele (status >= 400) -> projeksiyon -> dosya
static void threeStagePipelineWritesExpectedFile() {
    FileLineSource source(fixturePath);
    ParserStage parser;
    RunContext context;
    PolicyStage<std::string,LogRecord> guardedParser(parser,context,"parse",ErrorPolicy(ErrorPolicy::SKIP));
    PredicateFilterStage errors(Predicates::statusAtLeast(400));
    ProjectionStage projection(projectedFields());
    FileSink sink(OUT, LogFields::tabSeparated(projectedFields()));

    PipelineBuilder::from(source).via(guardedParser).via(errors).via(projection).to(sink).run();

    CHECK(readFile(OUT) ==
          "2024-01-01T10:00:01Z\t10.0.0.2\t500\t/api/users\n"
          "2024-01-01T10:00:02Z\t10.0.0.3\t401\t/api/login\n"
          "2024-01-01T10:00:03Z\t10.0.0.4\t404\t/missing.png\n"
          "2024-01-01T10:00:05Z\t10.0.0.6\t503\t/api/items/7\n");
    CHECK(parser.parsedCount() == 6 && parser.badCount() == 1);
    CHECK(errors.passedCount() == 4 && errors.droppedCount() == 2);
    CHECK(projection.projectedCount() == 4);
    CHECK(sink.writtenCount() == 4);
    std::remove(OUT);
}

// Her iki filtre (status >= 400 VE /api): sonuc iki sirada da ayni dosya icerigi.
static std::string runTwoFilters(bool statusFirst) {
    FileLineSource source(fixturePath);
    ParserStage parser;
    RunContext context;
    PolicyStage<std::string,LogRecord> guardedParser(parser,context,"parse",ErrorPolicy(ErrorPolicy::SKIP));
    PredicateFilterStage errors(Predicates::statusAtLeast(400));
    PredicateFilterStage api(Predicates::pathStartsWith("/api"));
    ProjectionStage projection(projectedFields());
    FileSink sink(OUT, LogFields::tabSeparated(projectedFields()));

    PipelineStep<LogRecord> chain = PipelineBuilder::from(source).via(guardedParser);
    chain = statusFirst ? chain.via(errors).via(api) : chain.via(api).via(errors);
    chain.via(projection).to(sink).run();

    std::string content = readFile(OUT);
    std::remove(OUT);
    return content;
}

static void bothFilterOrdersWriteIdenticalFiles() {
    std::string a = runTwoFilters(true);
    std::string b = runTwoFilters(false);
    CHECK(a == b);
    CHECK(a ==
          "2024-01-01T10:00:01Z\t10.0.0.2\t500\t/api/users\n"
          "2024-01-01T10:00:02Z\t10.0.0.3\t401\t/api/login\n"
          "2024-01-01T10:00:05Z\t10.0.0.6\t503\t/api/items/7\n");
}

// Filtre yok, projeksiyon yok: tum gecerli kayitlar varsayilan bicimde yazilir.
static void noFiltersWritesAllValidRecords() {
    FileLineSource source(fixturePath);
    ParserStage parser;
    RunContext context;
    PolicyStage<std::string,LogRecord> guardedParser(parser,context,"parse",ErrorPolicy(ErrorPolicy::SKIP));
    FileSink sink(OUT);

    PipelineBuilder::from(source).via(guardedParser).to(sink).run();

    CHECK(sink.writtenCount() == 6);
    CHECK(readFile(OUT).find("10.0.0.1 GET /index.html -> 200 (100 B)") != std::string::npos);
    std::remove(OUT);
}

// Hat hata verse bile (burada: girdi dosyasi yok) cikti dosyasi duzgun kapanir.
static void missingInputStillClosesOutput() {
    FileLineSource source("yok.log");
    ParserStage parser;
    RunContext context;
    PolicyStage<std::string,LogRecord> guardedParser(parser,context,"parse",ErrorPolicy(ErrorPolicy::SKIP));
    FileSink sink(OUT);
    Pipeline p = PipelineBuilder::from(source).via(guardedParser).to(sink);

    CHECK_THROWS(p.run(), PipelineException);
    CHECK(readFile(OUT).empty());                  // dosya olusturuldu, bos ve kapali
    std::remove(OUT);
}

int main(int argc, char* argv[]) {
    if (argc > 1) fixturePath = argv[1];
    run("3 asamali hat beklenen dosyayi yazar", threeStagePipelineWritesExpectedFile);
    run("iki filtre sirasi ayni dosyayi uretir", bothFilterOrdersWriteIdenticalFiles);
    run("filtresiz hat tum gecerli kayitlari yazar", noFiltersWritesAllValidRecords);
    run("girdi hatasinda cikti yine kapatilir", missingInputStillClosesOutput);
    return finish();
}
