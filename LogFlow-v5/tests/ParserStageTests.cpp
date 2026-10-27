// ParserStage birim testleri. Dosya sistemine dokunmaz: asamaya bir dizgi
// verilir, yayilan ciktilar CollectingEmitter ile yakalanir.
// Dis kutuphane gerekmez (Dev-C++ ile de derlenir).
#include <iostream>
#include <string>
#include "CollectingEmitter.h"
#include "../ParserStage.h"
#include "../ConsoleSink.h"

static int failures = 0, total = 0;

#define CHECK(cond) do { ++total; if (!(cond)) { ++failures; \
    std::cerr << "  BASARISIZ: " #cond " (satir " << __LINE__ << ")" << std::endl; } } while (0)

static void run(const char* name, void (*fn)()) {
    int before = failures;
    fn();
    std::cout << (failures == before ? "[GECTI]  " : "[KALDI]  ") << name << std::endl;
}

#define EXPECT_STAGE_ERROR(expr) do {bool caught=false;try {expr;}catch(const StageException&){caught=true;}CHECK(caught);}while(0)

static const char* GOOD =
    "192.168.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET /index.html HTTP/1.1\" 200 1234";

// 1. Gecerli satir
static void validLine() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    p.process(GOOD, out);
    CHECK(out.items.size() == 1);
    CHECK(p.parsedCount() == 1 && p.badCount() == 0);
    const LogRecord& r = out.items[0];
    CHECK(r.clientIp() == "192.168.1.1");
    CHECK(r.method() == "GET");
    CHECK(r.path() == "/index.html");
    CHECK(r.status() == 200);
    CHECK(r.bytes() == 1234);
    CHECK(r.timestamp() == 1704104130);          // 2024-01-01T10:15:30Z
    CHECK(r.raw() == GOOD);
    CHECK(r.attributes().empty());
    CHECK(r.userAgent().empty());
}

// 2. Eksik alan
static void missingField() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    EXPECT_STAGE_ERROR(p.process("192.168.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET /index.html HTTP/1.1\" 200", out));
    EXPECT_STAGE_ERROR(p.process("192.168.1.1 - -", out));
    EXPECT_STAGE_ERROR(p.process("192.168.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET\" 200 10", out));
    CHECK(out.items.empty());
    CHECK(p.badCount() == 3);
}

// 3. Hatali zaman damgasi
static void badTimestamp() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [32/Jan/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 200 1", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Foo/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 200 1", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024:25:15:30 +0000] \"GET / HTTP/1.1\" 200 1", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [30/Feb/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 200 1", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [bozuk zaman] \"GET / HTTP/1.1\" 200 1", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 *0000] \"GET / HTTP/1.1\" 200 1", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024 10:15:30 +0000] \"GET / HTTP/1.1\" 200 1", out));
    CHECK(out.items.empty());
    CHECK(p.badCount() == 7);
}

// 4. Hatali durum kodu
static void badStatus() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET / HTTP/1.1\" abc 10", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 99 10", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 700 10", out));
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 200 xyz", out));
    CHECK(out.items.empty());
    CHECK(p.badCount() == 4);
}

// 5. Bos satir
static void emptyLine() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    EXPECT_STAGE_ERROR(p.process("", out));
    EXPECT_STAGE_ERROR(p.process("   \t ", out));
    CHECK(out.items.empty());
    CHECK(p.badCount() == 2 && p.parsedCount() == 0);
}

// 6. Fazladan bosluk
static void extraWhitespace() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    p.process("  192.168.1.2   -  -   [01/Jan/2024:10:15:31 +0000]   \"GET  /style.css  HTTP/1.1\"   200    5678  ", out);
    CHECK(out.items.size() == 1);
    CHECK(out.items[0].clientIp() == "192.168.1.2");
    CHECK(out.items[0].path() == "/style.css");
    CHECK(out.items[0].bytes() == 5678);
}

// 7. Bosluk iceren tirnakli user agent
static void quotedUserAgent() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    p.process("10.0.0.1 - - [01/Jan/2024:10:15:30 +0000] \"GET /a HTTP/1.1\" 200 5 \"http://ref.example/\" \"Mozilla/5.0 (X11; Linux x86_64) Firefox/120.0\"", out);
    CHECK(out.items.size() == 1);
    CHECK(out.items[0].userAgent() == "Mozilla/5.0 (X11; Linux x86_64) Firefox/120.0");
    CHECK(ConsoleSink::format(out.items[0]).find("UA=") != std::string::npos);
}

// 8. Sorgu dizesi (query string)
static void queryString() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    p.process("10.0.0.1 - - [01/Jan/2024:10:15:30 +0000] \"GET /search?q=log+flow&page=2 HTTP/1.1\" 200 99", out);
    CHECK(out.items.size() == 1);
    CHECK(out.items[0].path() == "/search?q=log+flow&page=2");
}

// Ek: bayt "-", saat dilimi, artik yil, protokolsuz istek, sayaclar
static void extras() {
    ParserStage p; CollectingEmitter<LogRecord> out;
    p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 304 -", out);
    CHECK(out.items.size() == 1 && out.items[0].bytes() == 0);

    p.process("1.1.1.1 - - [01/Jan/2024:12:15:30 +0200] \"GET / HTTP/1.1\" 200 1", out);
    CHECK(out.items[1].timestamp() == 1704104130);     // +0200 -> ayni UTC ani
    p.process("1.1.1.1 - - [01/Jan/2024:05:15:30 -0500] \"GET / HTTP/1.1\" 200 1", out);
    CHECK(out.items[2].timestamp() == 1704104130);

    p.process("1.1.1.1 - - [29/Feb/2024:00:00:00 +0000] \"GET / HTTP/1.1\" 200 1", out);
    CHECK(out.items.size() == 4);                      // 2024 artik yil
    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [29/Feb/2023:00:00:00 +0000] \"GET / HTTP/1.1\" 200 1", out));
    CHECK(out.items.size() == 4 && p.badCount() == 1); // 2023 degil

    p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET /x\" 200 1", out);
    CHECK(out.items.size() == 5);                      // protokol istege bagli

    EXPECT_STAGE_ERROR(p.process("1.1.1.1 - - [01/Jan/2024:10:15:30 +0000] \"GET / HTTP/1.1\" 200 1 \"ref\" \"eksik", out));
    CHECK(out.items.size() == 5 && p.badCount() == 2);

    CHECK(ConsoleSink::format(out.items[0]) ==
          "2024-01-01T10:15:30Z 1.1.1.1 GET / -> 304 (0 B)");
}

// Ek: Stage open/close varsayilanlari ve ConsoleSink.consume
static void sinkAndLifecycle() {
    ParserStage p; p.open(); p.close();
    CollectingEmitter<LogRecord> out;
    p.process(GOOD, out);
    ConsoleSink sink;
    sink.consume(out.items[0]);                        // ciktiyi ekrana yazar
    CHECK(out.items.size() == 1);
}

int main() {
    run("gecerli satir", validLine);
    run("eksik alan", missingField);
    run("hatali zaman damgasi", badTimestamp);
    run("hatali durum kodu", badStatus);
    run("bos satir", emptyLine);
    run("fazladan bosluk", extraWhitespace);
    run("tirnakli user agent (bosluklu)", quotedUserAgent);
    run("sorgu dizesi", queryString);
    run("ek: bayt/saat dilimi/artik yil/protokol", extras);
    run("ek: sink ve yasam dongusu", sinkAndLifecycle);
    std::cout << "\n" << (total - failures) << "/" << total << " dogrulama gecti." << std::endl;
    return failures == 0 ? 0 : 1;
}
