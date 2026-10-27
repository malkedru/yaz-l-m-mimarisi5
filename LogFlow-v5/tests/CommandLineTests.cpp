// CommandLine (komut satiri arguman isleme) testleri.
#include "TestHarness.h"
#include "../CommandLine.h"

// {"logflow", "girdi.log", ...} biciminde bir arguman listesini ayristirir.
static Options parseArgs(const std::vector<const char*>& args) {
    return CommandLine::parse(static_cast<int>(args.size()), &args[0]);
}

static std::vector<const char*> args(const char* a = 0, const char* b = 0, const char* c = 0,
                                     const char* d = 0, const char* e = 0, const char* f = 0) {
    std::vector<const char*> v;
    v.push_back("logflow");
    const char* given[] = {a, b, c, d, e, f};
    for (int i = 0; i < 6; ++i) if (given[i]) v.push_back(given[i]);
    return v;
}

static void inputOnly() {
    Options o = parseArgs(args("in.log"));
    CHECK(o.input == "in.log");
    CHECK(!o.hasMinStatus && !o.hasPathPrefix && o.fields.empty() && o.output.empty());
}

static void fullCommandLine() {
    std::vector<const char*> v = args("in.log", "--min-status", "400", "--path-prefix", "/api");
    v.push_back("--fields"); v.push_back("timestamp,clientIp,status");
    v.push_back("--output"); v.push_back("out.tsv");
    Options o = parseArgs(v);

    CHECK(o.hasMinStatus && o.minStatus == 400);
    CHECK(o.hasPathPrefix && o.pathPrefix == "/api");
    CHECK(o.fields.size() == 3 && o.fields[0] == "timestamp" && o.fields[2] == "status");
    CHECK(o.output == "out.tsv");
}

static void optionsBeforeInput() {
    Options o = parseArgs(args("--min-status", "500", "in.log"));
    CHECK(o.input == "in.log" && o.minStatus == 500);
}

static void helpNeedsNoInput() {
    CHECK(parseArgs(args("--help")).help);
    CHECK(parseArgs(args("-h")).help);
    CHECK(parseArgs(args("--version")).version);
}

static void invalidUsageRejected() {
    CHECK_THROWS(parseArgs(args()), UsageError);                                  // girdi yok
    CHECK_THROWS(parseArgs(args("a.log", "b.log")), UsageError);                  // fazladan
    CHECK_THROWS(parseArgs(args("in.log", "--bilinmeyen")), UsageError);
    CHECK_THROWS(parseArgs(args("in.log", "--min-status")), UsageError);          // deger yok
    CHECK_THROWS(parseArgs(args("in.log", "--min-status", "abc")), UsageError);
    CHECK_THROWS(parseArgs(args("in.log", "--min-status", "99")), UsageError);
    CHECK_THROWS(parseArgs(args("in.log", "--min-status", "700")), UsageError);
    CHECK_THROWS(parseArgs(args("in.log", "--fields", "status,yok")), UsageError);
    CHECK_THROWS(parseArgs(args("in.log", "--fields", "status,")), UsageError);   // bos alan adi
    CHECK_THROWS(parseArgs(args("in.log", "--path-prefix")), UsageError);
}

int main() {
    run("yalnizca girdi dosyasi", inputOnly);
    run("tum secenekler birlikte", fullCommandLine);
    run("secenekler girdiden once gelebilir", optionsBeforeInput);
    run("--help ve --version girdi istemez", helpNeedsNoInput);
    run("gecersiz kullanim reddedilir", invalidUsageRejected);
    return finish();
}
