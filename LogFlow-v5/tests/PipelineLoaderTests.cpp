#include "TestHarness.h"
#include "TestSupport.h"
#include "../PipelineLoader.h"
#include "../CommandLine.h"
#include <cstdio>

static std::string config(const std::string& stages, const std::string& source="tests/fixtures/mini.log", const std::string& output="build/loader-result.tmp") {
    return "{\"pipeline\":{\"name\":\"test\",\"source\":{\"type\":\"file\",\"params\":{\"path\":\""+source+"\"}},\"stages\":"+stages+",\"sink\":{\"type\":\"file\",\"params\":{\"path\":\""+output+"\"}}}}";
}
static const std::string parse="{\"type\":\"parse\",\"params\":{\"format\":\"clf\"}}";
static const std::string project="{\"type\":\"project\",\"params\":{\"fields\":[\"clientIp\",\"bytes\"]}}";
static void invalid(const std::string& text,const std::string& expected) {
    try {PipelineLoader().loadText(text);CHECK(false);}
    catch(const ConfigError& e) {CHECK(std::string(e.what()).find(expected)!=std::string::npos);}
}
static void validAndLifetime() {
    Pipeline p=PipelineLoader().loadText(config("["+parse+","+project+"]","data/access-small.log"));
    p.run();
    std::string out=readFile("build/loader-result.tmp");
    CHECK(out.find("192.168.1.1\t1234\n")==0);
    CHECK(std::count(out.begin(),out.end(),'\n')==10);
    std::remove("build/loader-result.tmp");
}
static void requiredCases() {
    invalid(config("["+parse+",{\"type\":\"unknown\",\"params\":{}}]"),"unknown stage");
    invalid(config("[{\"type\":\"parse\",\"params\":{}}]"),"format: required");
    invalid(config("[{\"type\":\"parse\",\"params\":{\"format\":5}}]"),"string required");
    invalid(config("[]"),"non-empty array");
    invalid(config("["+parse+",{\"type\":\"filter\",\"params\":{\"field\":\"status\",\"op\":\">=\",\"value\":\"400\"}}]"),"integer required");
}
static void validationBeforeIO() {
    const std::string target="build/loader-untouched.tmp";
    {std::ofstream out(target.c_str());out<<"keep me";}
    invalid(config("["+parse+",{\"type\":\"oops\",\"params\":{}}]","does-not-exist.log",target),"unknown stage");
    CHECK(readFile(target)=="keep me");
    std::remove(target.c_str());
    Pipeline p=PipelineLoader().loadText(config("["+parse+"]","does-not-exist.log"));
    CHECK_THROWS(p.run(),PipelineException);
    std::remove("build/loader-result.tmp");
}
static void topologyAndSchema() {
    invalid(config("["+project+"]"),"first stage");
    invalid(config("["+parse+","+parse+"]"),"only allowed first");
    invalid(config("["+parse+","+project+",{\"type\":\"filter\",\"params\":{\"field\":\"status\",\"op\":\">=\",\"value\":400}}]"),"removed by projection");
    invalid(config("["+parse+",{\"type\":\"project\",\"params\":{\"fields\":[\"oops\"]}}]"),"unknown or duplicate field");
    invalid(config("["+parse+",{\"type\":\"project\",\"params\":{\"fields\":[\"path\",\"path\"]}}]"),"duplicate field");
    invalid(config("["+parse+",{\"type\":\"filter\",\"params\":{\"field\":\"status\",\"op\":\">=\",\"value\":400.0}}]"),"integer required");
    invalid(config("["+parse+",{\"type\":\"filter\",\"params\":{\"field\":\"bytes\",\"op\":\"startsWith\",\"value\":4}}]"),"text field");
    invalid(config("["+parse+"]","a.log","./a.log"),"different files");
    invalid("{\"pipeline\":{},\"pipeline\":{}}","duplicate key");
    invalid("{\"pipeline\":{} } trailing","trailing content");
    invalid(config("{}"),"non-empty array");
    invalid("{\"pipline\":{}}","unknown parameter");
    CHECK_THROWS(PipelineLoader().load("missing-config.json"),ConfigError);
}
static void jsonSyntax() {
    const char* bad[]={"", "[1,]", "{\"a\":01}", "{\"a\":true,}", "[+1]", "[1.]", "[1e]", "\"\\uD800\"", "\"\\uDC00\"", "\"\\x\""};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) CHECK_THROWS(ConfigParser(bad[i]).parse(),ConfigError);
    CHECK(ConfigParser("\"\\u0130\\uD83D\\uDE00\"").parse().text=="\xC4\xB0\xF0\x9F\x98\x80");
    CHECK(ConfigParser("[true,false,null,-12,1.5,1e2]").parse().array.size()==6);
}
static void registration() {
    StageRegistry r=BuiltinStages::registry();
    CHECK_THROWS(r.add("parse",BuiltinStages::parse),ConfigError);
    r.add("select",BuiltinStages::project);
    Pipeline p=PipelineLoader(r).loadText(config("["+parse+",{\"type\":\"select\",\"params\":{\"fields\":[\"path\"]}}]","data/access-small.log"));
    p.run();CHECK(readFile("build/loader-result.tmp").find("/index.html\n")==0);
    std::remove("build/loader-result.tmp");
}
static void examples() {
    PipelineLoader loader;
    loader.load("configs/error-report.yaml").run();
    loader.load("configs/api-traffic.json").run();
    loader.load("configs/bandwidth-by-client.json").run();
    CHECK(readFile("out/errors.txt")=="2024-01-01T10:15:34Z\t192.168.1.4\t404\t/logo.png\n");
    CHECK(readFile("out/api-traffic.tsv")=="2024-01-01T10:15:32Z\tPOST\t/api/login\t201\n2024-01-01T10:15:35Z\tPOST\t/api/data\t200\n2024-01-01T10:15:38Z\tDELETE\t/api/user/5\t204\n");
    std::string b=readFile("out/bandwidth-by-client.tsv");
    CHECK(std::count(b.begin(),b.end(),'\n')==10);
    CHECK(b.find("192.168.1.2\t8192\n")!=std::string::npos);
    CHECK(b!=readFile("out/api-traffic.tsv"));
}
static void cli() {
    const char* good[]={"logflow","--config","configs/error-report.yaml"};
    CHECK(CommandLine::parse(3,good).config=="configs/error-report.yaml");
    const char* missing[]={"logflow","--config"};CHECK_THROWS(CommandLine::parse(2,missing),UsageError);
    const char* mixed[]={"logflow","--config","a.json","data.log"};CHECK_THROWS(CommandLine::parse(4,mixed),UsageError);
}
int main() {
    run("valid config and component ownership",validAndLifetime);
    run("five mandatory loader cases",requiredCases);
    run("fail fast before input/output and runtime distinction",validationBeforeIO);
    run("topology and schema errors",topologyAndSchema);
    run("strict JSON syntax and Unicode",jsonSyntax);
    run("registry extension without loader modification",registration);
    run("three different outputs from one program",examples);
    run("config command line",cli);
    return finish();
}
