// PredicateFilterStage ve hazir kosullarin (Predicates) birim testleri.
#include "TestHarness.h"
#include "TestSupport.h"
#include "CollectingEmitter.h"
#include "../PredicateFilterStage.h"

static std::vector<LogRecord> feed(PredicateFilterStage& stage, const std::vector<LogRecord>& in) {
    CollectingEmitter<LogRecord> out;
    for (size_t i = 0; i < in.size(); ++i) stage.process(in[i], out);
    return out.items;
}

static void statusCondition() {
    std::vector<LogRecord> in;
    const int codes[] = {200, 399, 400, 404, 500};
    for (int i = 0; i < 5; ++i) in.push_back(makeRecord(i, "1.1.1.1", "/x", codes[i]));

    PredicateFilterStage stage(Predicates::statusAtLeast(400));
    std::vector<LogRecord> out = feed(stage, in);

    CHECK(out.size() == 3);
    CHECK(out[0].status() == 400 && out[1].status() == 404 && out[2].status() == 500);
    CHECK(stage.evaluatedCount() == 5);
    CHECK(stage.passedCount() == 3);
    CHECK(stage.droppedCount() == 2);
}

static void pathPrefixCondition() {
    std::vector<LogRecord> in;
    in.push_back(makeRecord(0, "1.1.1.1", "/api/users", 200));
    in.push_back(makeRecord(1, "1.1.1.1", "/api", 200));            // onekin kendisi
    in.push_back(makeRecord(2, "1.1.1.1", "/ap", 200));             // onekten kisa
    in.push_back(makeRecord(3, "1.1.1.1", "/v2/api/users", 200));   // onek basta degil
    in.push_back(makeRecord(4, "1.1.1.1", "/API/users", 200));      // buyuk/kucuk harf duyarli

    PredicateFilterStage stage(Predicates::pathStartsWith("/api"));
    std::vector<LogRecord> out = feed(stage, in);

    CHECK(out.size() == 2);
    CHECK(out[0].path() == "/api/users" && out[1].path() == "/api");
}

static void timeRangeCondition() {
    std::vector<LogRecord> in;
    for (int t = 100; t <= 105; ++t) in.push_back(makeRecord(t, "1.1.1.1", "/x", 200));

    PredicateFilterStage stage(Predicates::timestampBetween(102, 104));
    std::vector<LogRecord> out = feed(stage, in);

    CHECK(out.size() == 3);                                  // iki uc dahil
    CHECK(out[0].timestamp() == 102 && out[2].timestamp() == 104);
}

static void passesRecordUnchanged() {
    LogRecord rec = makeRecord(7, "9.9.9.9", "/same", 500, 42);
    PredicateFilterStage stage(Predicates::statusAtLeast(100));
    CollectingEmitter<LogRecord> out;
    stage.process(rec, out);

    CHECK(out.items.size() == 1);
    CHECK(out.items[0].raw() == rec.raw() && out.items[0].bytes() == 42);
    CHECK(out.items[0].userAgent() == "test-agent");         // hicbir alan degismez
}

static void customLambdaCondition() {
    PredicateFilterStage stage([](const LogRecord& r) { return r.bytes() > 1000; });
    std::vector<LogRecord> in;
    in.push_back(makeRecord(0, "1.1.1.1", "/small", 200, 10));
    in.push_back(makeRecord(1, "1.1.1.1", "/big", 200, 5000));
    std::vector<LogRecord> out = feed(stage, in);
    CHECK(out.size() == 1 && out[0].path() == "/big");
}

static void emptyPredicateRejected() {
    CHECK_THROWS(PredicateFilterStage stage((LogPredicate())), StageException);
}

static void lifecycleDefaults() {
    PredicateFilterStage stage(Predicates::statusAtLeast(400));
    stage.open();
    stage.close();                                           // varsayilan: hicbir sey yapmaz
    CHECK(stage.evaluatedCount() == 0);
}

int main() {
    run("durum kodu kosulu (status >= 400)", statusCondition);
    run("yol oneki kosulu (startsWith /api)", pathPrefixCondition);
    run("zaman araligi kosulu (iki uc dahil)", timeRangeCondition);
    run("kaydi degistirmeden iletir", passesRecordUnchanged);
    run("ozel lambda kosulu", customLambdaCondition);
    run("bos kosul reddedilir", emptyPredicateRejected);
    run("yasam dongusu varsayilanlari", lifecycleDefaults);
    return finish();
}
