#include "TestHarness.h"
#include "TestSupport.h"
#include "../PipelineLoader.h"
#include "CollectingEmitter.h"
#include <cstdio>

static std::string replaceAll(std::string s,const std::string& a,const std::string& b) {
    size_t p=0;while((p=s.find(a,p))!=std::string::npos){s.replace(p,a.size(),b);p+=b.size();}return s;
}
static size_t lines(const std::string& s){return std::count(s.begin(),s.end(),'\n');}
static std::string fixtureConfig(){return readFile("configs/dirty-dead-letter.json");}
static void chaos() {
    Pipeline p=PipelineLoader().load("configs/dirty-dead-letter.json");p.run();
    const RunSummary& s=*p.summary();
    CHECK(s.completed);CHECK(s.received==11);CHECK(s.emitted==3);CHECK(s.deadLettered()==8);CHECK(s.dropped()==0);CHECK(s.aborted==0);CHECK(s.reconciles());CHECK(s.elapsedMs>=0);
    CHECK(s.stages.at("pipeline.stages[0]:parse").deadLettered==6);CHECK(s.stages.at("pipeline.stages[1]:validate").deadLettered==2);
    const std::string expected="2026-10-15T10:15:32Z\t192.0.2.10\tGET\t/index.html\t200\t1234\n2026-10-15T10:16:05Z\t198.51.100.22\tPOST\t/api/login\t401\t287\n2026-10-15T10:17:41Z\t203.0.113.7\tGET\t/images/logo.png\t304\t0\n";
    CHECK(readFile("out/valid.tsv")==expected);
    std::istringstream in(readFile("out/dead-letter.jsonl"));std::string line;std::vector<std::string> raw;
    while(std::getline(in,line)) {
        ConfigValue r=ConfigParser(line).parse();CHECK(r.at("stageName","dl").text.find("pipeline.stages[")==0);
        CHECK(!r.at("exception","dl").text.empty());CHECK(r.at("timestamp","dl").text.size()==20);
        CHECK(r.at("rawEncoding","dl").text=="latin-1-bytes");raw.push_back(r.at("rawInput","dl").text);
    }
    CHECK(raw.size()==8);
    CHECK(raw[0].find("# Deliberately")==0);CHECK(raw[1].find("10:18:")!=std::string::npos);
    CHECK(raw[2].find("-45")!=std::string::npos);CHECK(raw[3].find("1970")!=std::string::npos);CHECK(raw[4].find("2099")!=std::string::npos);
    CHECK(raw[5].find("/broken")!=std::string::npos);
    CHECK(raw[6].find("\xC3\xBF\xC3\xBE\xC3\xBA")!=std::string::npos);CHECK(raw[7].size()==102400);
    CHECK(lines(readFile("out/dead-letter.jsonl"))==8);
    p.run();CHECK(p.summary()->received==11);CHECK(p.summary()->deadLettered()==8);CHECK(readFile("out/valid.tsv")==expected);CHECK(lines(readFile("out/dead-letter.jsonl"))==8);
}
static void otherPoliciesAndEmpty() {
    Pipeline p=PipelineLoader().load("configs/dirty-skip.json");p.run();
    CHECK(p.summary()->received==11);CHECK(p.summary()->emitted==3);CHECK(p.summary()->dropped()==8);CHECK(p.summary()->deadLettered()==0);CHECK(p.summary()->reconciles());
    Pipeline f=PipelineLoader().load("configs/dirty-fail-fast.json");CHECK_THROWS(f.run(),PipelineException);
    CHECK(!f.summary()->completed);CHECK(f.summary()->received==1);CHECK(f.summary()->aborted==1);CHECK(f.summary()->reconciles());
    Pipeline e=PipelineLoader().load("configs/empty.json");e.run();CHECK(e.summary()->completed);CHECK(e.summary()->received==0);CHECK(e.summary()->emitted==0);CHECK(e.summary()->reconciles());CHECK(readFile("out/empty.tsv").empty());CHECK(readFile("out/empty-dead-letter.jsonl").empty());
}
class Flaky : public Stage<LogRecord,LogRecord> {
public:
    int attempts=0;int failures;bool fatal;
    explicit Flaky(int count,bool f=false):failures(count),fatal(f) {}
    void process(const LogRecord& r,Emitter<LogRecord>& out) override {
        ++attempts;out.emit(r);
        if(attempts<=failures) {if(fatal)throw PipelineException("fatal I/O");throw StageException("temporary failure");}
    }
};
static void retryIsolation() {
    LogRecord r=makeRecord(1704104130,"127.0.0.1","/",200,1);
    RunContext ctx;Flaky transient(2);PolicyStage<LogRecord,LogRecord> retry(transient,ctx,"io",ErrorPolicy(ErrorPolicy::RETRY,2));
    CollectingEmitter<LogRecord> out;retry.process(r,out);
    CHECK(transient.attempts==3);CHECK(out.items.size()==1);CHECK(ctx.summary.stages["io"].retries==2);CHECK(ctx.summary.stages["io"].errors==2);CHECK(ctx.summary.stages["io"].failed==0);
    Flaky broken(5);PolicyStage<LogRecord,LogRecord> exhausted(broken,ctx,"broken",ErrorPolicy(ErrorPolicy::RETRY,2));
    CHECK_THROWS(exhausted.process(r,out),PipelineException);CHECK(broken.attempts==3);CHECK(out.items.size()==1);CHECK(ctx.summary.stages["broken"].failed==1);
    Flaky fatal(1,true);PolicyStage<LogRecord,LogRecord> fatalRetry(fatal,ctx,"fatal",ErrorPolicy(ErrorPolicy::RETRY,4));
    CHECK_THROWS(fatalRetry.process(r,out),PipelineException);CHECK(fatal.attempts==1);CHECK(out.items.size()==1);
    Flaky once(2);PolicyStage<LogRecord,LogRecord> noRetries(once,ctx,"zero",ErrorPolicy(ErrorPolicy::RETRY,0));
    CHECK_THROWS(noRetries.process(r,out),PipelineException);CHECK(once.attempts==1);
    Flaky downstream(1);PolicyStage<LogRecord,LogRecord> skip(downstream,ctx,"downstream",ErrorPolicy(ErrorPolicy::SKIP));
    class Forward : public Emitter<LogRecord> {
        Stage<LogRecord,LogRecord>& s_;Emitter<LogRecord>& o_;
    public: Forward(Stage<LogRecord,LogRecord>& s,Emitter<LogRecord>& o):s_(s),o_(o){}void emit(const LogRecord& r)override{s_.process(r,o_);}
    } forward(skip,out);
    Flaky upstream(0);PolicyStage<LogRecord,LogRecord> parent(upstream,ctx,"upstream",ErrorPolicy(ErrorPolicy::RETRY,5));parent.process(r,forward);
    CHECK(upstream.attempts==1);CHECK(downstream.attempts==1);CHECK(ctx.summary.stages["downstream"].dropped==1);CHECK(ctx.summary.stages["upstream"].retries==0);CHECK(out.items.size()==1);
}
static void validation() {
    ConfigValue rules=ConfigParser("{\"required\":[\"method\",\"path\",\"clientIp\"],\"ranges\":{\"bytes\":{\"min\":0,\"max\":100},\"status\":{\"min\":100,\"max\":599}},\"timestamp\":{\"min\":100,\"max\":200},\"ip\":true}").parse();
    ValidationStage v(rules,"rules");CollectingEmitter<LogRecord> out;
    v.process(makeRecord(100,"127.0.0.1","/",200,0),out);v.process(makeRecord(200,"2001:db8::1","/",599,100),out);CHECK(out.items.size()==2);
    CHECK_THROWS(v.process(makeRecord(99,"127.0.0.1","/",200),out),StageException);
    CHECK_THROWS(v.process(makeRecord(201,"127.0.0.1","/",200),out),StageException);
    CHECK_THROWS(v.process(makeRecord(150,"999.1.1.1","/",200),out),StageException);
    CHECK_THROWS(v.process(makeRecord(150,"127.0.0.1","",200),out),StageException);
    CHECK_THROWS(v.process(makeRecord(150,"127.0.0.1","/",200,-1),out),StageException);
    CHECK_THROWS(v.process(makeRecord(150,"127.0.0.1","/",200,101),out),StageException);
    CHECK_THROWS(v.process(makeRecord(150,"127.0.0.1","/",600),out),StageException);
    const char* good[]={"0.0.0.0","255.255.255.255","::","::1","::ffff:192.0.2.1","2001:db8:1:2:3:4:5:6"};
    for(auto ip:good)CHECK(ValidationStage::validIp(ip));
    const char* bad[]={"","1.2.3","1.2.3.4.5","01.2.3.4","-1.2.3.4","256.0.0.1",":::1","1::2::3","1:2:3:4:5:6:7","1:2:3:4:5:6:7:8:9",":1:2:3:4:5:6:7","gggg::1"};
    for(auto ip:bad)CHECK(!ValidationStage::validIp(ip));
}
static void schemaAndFatalIO() {
    std::string c=fixtureConfig();
    const char* invalid[]={"UNKNOWN","retry"};for(auto policy:invalid)CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"DEAD_LETTER",policy)),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"\"DEAD_LETTER\"","{\"mode\":\"RETRY\",\"retries\":-1}")),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"\"DEAD_LETTER\"","{\"mode\":\"RETRY\",\"retries\":1.5}")),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"out/dead-letter.jsonl","out/valid.tsv")),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"out/dead-letter.jsonl","data/access-dirty.log")),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"\"maxLineBytes\": 65536","\"maxLineBytes\": 0")),ConfigError);
    CHECK_THROWS(ValidationStage(ConfigParser("{\"ranges\":{\"bytes\":{\"min\":2,\"max\":1}}}").parse(),"v"),ConfigError);
    CHECK_THROWS(ValidationStage(ConfigParser("{\"ip\":\"true\"}").parse(),"v"),ConfigError);
    CHECK_THROWS(ValidationStage(ConfigParser("{\"required\":[\"oops\"]}").parse(),"v"),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText("{\"pipeline\":{\"name\":\"missing-dl\",\"source\":{\"type\":\"file\",\"params\":{\"path\":\"missing.log\"}},\"stages\":[{\"type\":\"parse\",\"params\":{\"format\":\"clf\"},\"onError\":\"DEAD_LETTER\"}],\"sink\":{\"type\":\"console\",\"params\":{}}}}"),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"\"maxLineBytes\": 65536","\"maxLineBytes\": 10485761")),ConfigError);
    CHECK_THROWS(PipelineLoader().loadText(replaceAll(c,"\"DEAD_LETTER\"","{\"mode\":\"RETRY\",\"retries\":1001}")),ConfigError);
    // Missing branch is checked even when input does not exist; no output opens.
    std::string missing=replaceAll(c,"\"onError\": \"DEAD_LETTER\"","\"onError\": \"SKIP\"");
    Pipeline dlFail=PipelineLoader().loadText(replaceAll(c,"out/dead-letter.jsonl","does-not-exist/dead-letter.jsonl"));
    CHECK_THROWS(dlFail.run(),PipelineException);CHECK(!dlFail.summary()->completed);CHECK(dlFail.summary()->received==0);
    Pipeline sourceFail=PipelineLoader().loadText(replaceAll(missing,"data/access-dirty.log","missing.log"));
    CHECK_THROWS(sourceFail.run(),PipelineException);CHECK(sourceFail.summary()->reconciles());
    Pipeline sinkFail=PipelineLoader().loadText(replaceAll(missing,"out/valid.tsv","does-not-exist/output.tsv"));
    CHECK_THROWS(sinkFail.run(),PipelineException);CHECK(sinkFail.summary()->received==0);
}
static void utf8AndRawAfterProjection() {
    CHECK(ParserStage::validUtf8("\xC4\xB0\xF0\x9F\x98\x80"));
    const char* bad[]={"\xFF","\xC0\xAF","\xED\xA0\x80","\xF4\x90\x80\x80","\xE2\x82"};
    for(auto b:bad)CHECK(!ParserStage::validUtf8(b));
    ParserStage parser;CollectingEmitter<LogRecord> out;CHECK_THROWS(parser.process("bad",out),StageException);CHECK(parser.badCount()==1);
    RunContext ctx;ctx.rawInput=std::string("original\xFF\n\"",11);ctx.deadLetter.reset(new DeadLetterSink("build/raw-dl.tmp"));ctx.deadLetter->open();
    Flaky fail(1);PolicyStage<LogRecord,LogRecord> guarded(fail,ctx,"after-project",ErrorPolicy(ErrorPolicy::DEAD_LETTER));
    guarded.process(makeRecord(1,"","",0),out);ctx.deadLetter->close();
    ConfigValue d=ConfigParser(readFile("build/raw-dl.tmp")).parse();CHECK(d.at("rawInput","d").text.find("original") == 0);CHECK(d.at("rawInput","d").text.find("\xC3\xBF")!=std::string::npos);CHECK(out.items.empty());CHECK(ctx.summary.deadLettered()==1);std::remove("build/raw-dl.tmp");
}

static void lifecycleAndBranchFailure() {
    RunContext ctx;std::vector<std::string> events;
    TraceStage<LogRecord> opening("bad",events,true);
    PolicyStage<LogRecord,LogRecord> guarded(opening,ctx,"opening",ErrorPolicy(ErrorPolicy::SKIP));
    CHECK_THROWS(guarded.open(),PipelineException);
    class FailingBranch : public DeadLetterSink {
    public:
        FailingBranch():DeadLetterSink("unused") {}
        void consume(const FailedRecord&) override {throw PipelineException("branch disk full");}
    };
    ctx.deadLetter.reset(new FailingBranch);ctx.rawInput="original";
    Flaky stage(1);PolicyStage<LogRecord,LogRecord> dl(stage,ctx,"dl",ErrorPolicy(ErrorPolicy::DEAD_LETTER));
    CollectingEmitter<LogRecord> out;CHECK_THROWS(dl.process(makeRecord(1,"127.0.0.1","/",200),out),PipelineException);
    CHECK(ctx.summary.deadLettered()==0);CHECK(ctx.summary.stages["dl"].failed==1);CHECK(out.items.empty());
    class BrokenSink : public Sink<LogRecord> {
    public:void consume(const LogRecord&) override {throw PipelineException("main disk full");}
    } broken;
    RunContext mainContext;ListSource<std::string> source;source.items.push_back("127.0.0.1 - - [01/Jan/2024:00:00:00 +0000] \"GET / HTTP/1.1\" 200 1");
    CountingSource counted(source,mainContext);CountingSink sink(broken,mainContext);ParserStage parser;
    PolicyStage<std::string,LogRecord> retry(parser,mainContext,"parse",ErrorPolicy(ErrorPolicy::RETRY,5));
    Pipeline p=PipelineBuilder::from(counted).via(retry).to(sink);
    CHECK_THROWS(runWithSummary(mainContext,[&p](){p.run();}),PipelineException);
    CHECK(mainContext.summary.emitted==0);CHECK(mainContext.summary.aborted==1);CHECK(mainContext.summary.reconciles());CHECK(mainContext.summary.stages["parse"].retries==0);CHECK(parser.parsedCount()==1);
}

int main(){
    run("dirty fixture: exact output, all errors and repeated run",chaos);
    run("skip, fail fast and empty input",otherPoliciesAndEmpty);
    run("retry budget, atomic forwarding and downstream isolation",retryIsolation);
    run("declarative validation boundaries and IPv4/IPv6",validation);
    run("configuration errors and fatal I/O",schemaAndFatalIO);
    run("UTF-8 checks and original bytes after projection",utf8AndRawAfterProjection);
    run("lifecycle and independent branch failures stay fatal",lifecycleAndBranchFailure);
    return finish();
}
