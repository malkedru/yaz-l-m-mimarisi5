// ProjectionStage ve alan bicimleyicisi (LogFields) birim testleri.
#include "TestHarness.h"
#include "TestSupport.h"
#include "CollectingEmitter.h"
#include "../ProjectionStage.h"
#include "../PredicateFilterStage.h"

static std::vector<std::string> fieldList(const char* a, const char* b = 0, const char* c = 0) {
    std::vector<std::string> fields;
    fields.push_back(a);
    if (b) fields.push_back(b);
    if (c) fields.push_back(c);
    return fields;
}

static LogRecord fullRecord() {
    std::map<std::string, std::string> attrs;
    attrs["country"] = "TR";
    return LogRecord(1704104130, "10.0.0.7", "POST", "/api/login?u=ali", 401, 256,
                     "Mozilla/5.0", attrs, "tam ham satir");
}

static void keepsOnlySelectedFields() {
    ProjectionStage stage(fieldList("clientIp", "status"));
    CollectingEmitter<LogRecord> out;
    stage.process(fullRecord(), out);

    CHECK(out.items.size() == 1);
    const LogRecord& r = out.items[0];
    CHECK(r.clientIp() == "10.0.0.7");                       // secilenler korunur
    CHECK(r.status() == 401);
    CHECK(r.path().empty() && r.method().empty());           // secilmeyenler bosalir
    CHECK(r.timestamp() == 0 && r.bytes() == 0);
    CHECK(r.userAgent().empty());
    CHECK(stage.projectedCount() == 1);
}

static void dropsRawAndAttributes() {
    // Ham satir, secilmeyen tum alanlari icerir; sizmamali (veri minimizasyonu).
    ProjectionStage stage(fieldList("status"));
    CollectingEmitter<LogRecord> out;
    stage.process(fullRecord(), out);
    CHECK(out.items[0].raw().empty());
    CHECK(out.items[0].attributes().empty());
}

static void allFieldsKeepsEverythingExceptRaw() {
    ProjectionStage stage(LogFields::names());
    CollectingEmitter<LogRecord> out;
    stage.process(fullRecord(), out);
    const LogRecord& r = out.items[0];
    CHECK(r.timestamp() == 1704104130 && r.clientIp() == "10.0.0.7");
    CHECK(r.method() == "POST" && r.path() == "/api/login?u=ali");
    CHECK(r.status() == 401 && r.bytes() == 256 && r.userAgent() == "Mozilla/5.0");
    CHECK(r.raw().empty());
}

static void projectionIsIdempotent() {
    ProjectionStage stage(fieldList("path", "status"));
    CollectingEmitter<LogRecord> once, twice;
    stage.process(fullRecord(), once);
    stage.process(once.items[0], twice);
    CHECK(LogFields::tabSeparated(LogFields::names())(once.items[0]) ==
          LogFields::tabSeparated(LogFields::names())(twice.items[0]));
}

static void invalidFieldsRejected() {
    CHECK_THROWS(ProjectionStage stage(fieldList("clientIp", "olmayanAlan")), StageException);
    std::vector<std::string> none;
    CHECK_THROWS(ProjectionStage stage(none), StageException);
}

static void formatterFollowsRequestedOrder() {
    LogRecord r = fullRecord();
    CHECK(LogFields::tabSeparated(fieldList("status", "timestamp", "path"))(r) ==
          "401\t2024-01-01T10:15:30Z\t/api/login?u=ali");
    CHECK(LogFields::tabSeparated(fieldList("bytes", "method", "userAgent"))(r) ==
          "256\tPOST\tMozilla/5.0");
    CHECK_THROWS(LogFields::valueOf(r, "yok"), StageException);
}

// Tuzak: projeksiyondan SONRA gelen filtre, atilmis alani goremez.
static void filterAfterProjectionSeesDroppedFieldAsZero() {
    ProjectionStage projection(fieldList("clientIp"));       // status atiliyor
    PredicateFilterStage filter(Predicates::statusAtLeast(400));

    CollectingEmitter<LogRecord> projected, passed;
    projection.process(fullRecord(), projected);             // gercek status = 401
    filter.process(projected.items[0], passed);

    CHECK(passed.items.empty());                             // 401 >= 400 olmasina ragmen elendi
}

int main() {
    run("yalnizca secilen alanlar korunur", keepsOnlySelectedFields);
    run("ham satir ve attributes atilir", dropsRawAndAttributes);
    run("tum alanlar secilince raw disinda hepsi korunur", allFieldsKeepsEverythingExceptRaw);
    run("projeksiyon idempotent", projectionIsIdempotent);
    run("gecersiz alan / bos liste reddedilir", invalidFieldsRejected);
    run("bicimleyici istenen sirayi izler", formatterFollowsRequestedOrder);
    run("projeksiyondan sonra filtre atilmis alani goremez", filterAfterProjectionSeesDroppedFieldAsZero);
    return finish();
}
