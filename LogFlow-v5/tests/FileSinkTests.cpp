// FileSink birim testleri: yasam dongusu (open/consume/close) ve hata durumlari.
#include <cstdio>
#include "TestHarness.h"
#include "TestSupport.h"
#include "../FileSink.h"

static const char* TMP = "filesink_test.tmp";

static void writesLinesAfterOpenAndClose() {
    FileSink sink(TMP);
    sink.open();
    sink.consume(makeRecord(1704104130, "1.1.1.1", "/a", 200, 10));
    sink.consume(makeRecord(1704104131, "2.2.2.2", "/b", 404, 0));
    sink.close();

    CHECK(readFile(TMP) ==
          "2024-01-01T10:15:30Z 1.1.1.1 GET /a -> 200 (10 B) UA=\"test-agent\"\n"
          "2024-01-01T10:15:31Z 2.2.2.2 GET /b -> 404 (0 B) UA=\"test-agent\"\n");
    CHECK(sink.writtenCount() == 2);
    std::remove(TMP);
}

static void customFormatter() {
    FileSink sink(TMP, LogFields::tabSeparated(std::vector<std::string>(1, "path")));
    sink.open();
    sink.consume(makeRecord(0, "1.1.1.1", "/only-path", 200));
    sink.close();
    CHECK(readFile(TMP) == "/only-path\n");
    std::remove(TMP);
}

static void consumeBeforeOpenFails() {
    FileSink sink(TMP);
    CHECK_THROWS(sink.consume(makeRecord(0, "1.1.1.1", "/a", 200)), PipelineException);
    CHECK(readFile(TMP).empty());
    std::remove(TMP);
}

static void openFailureIsReported() {
    FileSink sink("bu-dizin-yok/cikti.txt");
    CHECK_THROWS(sink.open(), PipelineException);
}

static void closeTwiceIsHarmless() {
    FileSink sink(TMP);
    sink.open();
    sink.close();
    sink.close();                                            // hata vermemeli
    CHECK(readFile(TMP).empty());
    std::remove(TMP);
}

static void closeWithoutOpenIsHarmless() {
    FileSink sink(TMP);
    sink.close();
    CHECK(readFile(TMP).empty());                            // dosya hic olusmadi
}

static void openTruncatesExistingFile() {
    { std::ofstream old(TMP); old << "eski icerik\n"; }
    FileSink sink(TMP);
    sink.open();
    sink.consume(makeRecord(0, "1.1.1.1", "/yeni", 200));
    sink.close();
    CHECK(readFile(TMP).find("eski icerik") == std::string::npos);
    CHECK(readFile(TMP).find("/yeni") != std::string::npos);
    std::remove(TMP);
}

static void reopenStartsFresh() {
    FileSink sink(TMP);
    sink.open();
    sink.consume(makeRecord(0, "1.1.1.1", "/bir", 200));
    sink.close();
    sink.open();
    sink.consume(makeRecord(0, "1.1.1.1", "/iki", 200));
    sink.close();
    CHECK(sink.writtenCount() == 1);
    CHECK(readFile(TMP).find("/bir") == std::string::npos);
    std::remove(TMP);
}

int main() {
    run("open + consume + close satirlari yazar", writesLinesAfterOpenAndClose);
    run("ozel bicimleyici", customFormatter);
    run("open() oncesi consume() hata verir", consumeBeforeOpenFails);
    run("acilamayan dosya bildirilir", openFailureIsReported);
    run("close() iki kez cagrilabilir", closeTwiceIsHarmless);
    run("open() olmadan close() zararsiz", closeWithoutOpenIsHarmless);
    run("open() eski icerigi siler", openTruncatesExistingFile);
    run("yeniden open() temiz baslar", reopenStartsFresh);
    return finish();
}
