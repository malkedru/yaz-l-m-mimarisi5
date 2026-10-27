#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H
#include "Stage.h"
#include "Source.h"
#include "Sink.h"
#include "LogRecord.h"
#include "LogFields.h"
#include "ConfigValue.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <iomanip>

struct FailedRecord {
    std::string rawInput, stageName, exception;
    std::time_t timestamp;
    FailedRecord(const std::string& raw, const std::string& stage, const std::string& error)
        : rawInput(raw), stageName(stage), exception(error), timestamp(std::time(0)) {}
};
// ASCII JSON encoding preserves every raw byte, including invalid UTF-8, reversibly.
// Decode rawInput with latin-1 (one code point per original byte), not UTF-8.
inline std::string byteJson(const std::string& text) {
    std::ostringstream out;out << '"';
    for(size_t i=0;i<text.size();++i) {
        unsigned char c=static_cast<unsigned char>(text[i]);
        if(c=='"' || c=='\\') out << '\\' << static_cast<char>(c);
        else if(c<32 || c>=127) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
        else out << static_cast<char>(c);
    }
    out << '"';return out.str();
}
class DeadLetterSink : public Sink<FailedRecord> {
    std::string path_;std::ofstream out_;
public:
    explicit DeadLetterSink(const std::string& path):path_(path) {}
    void open() override {
        out_.open(path_.c_str(),std::ios::binary|std::ios::trunc);
        if(!out_) throw PipelineException("dead-letter open failed: "+path_);
    }
    void consume(const FailedRecord& r) override {
        out_ << "{\"rawInput\":" << byteJson(r.rawInput) << ",\"rawEncoding\":\"latin-1-bytes\",\"stageName\":" << byteJson(r.stageName)
             << ",\"exception\":" << byteJson(r.exception) << ",\"timestamp\":" << byteJson(LogFields::formatTimestamp(r.timestamp)) << "}\n";
        if(!out_) throw PipelineException("dead-letter write failed: "+path_);
    }
    void close() override {
        if(!out_.is_open()) return;
        out_.flush();bool ok=out_.good();out_.close();
        if(!ok || out_.fail()) throw PipelineException("dead-letter close failed: "+path_);
    }
};
struct StageMetrics {
    long long dropped=0, deadLettered=0, failed=0, retries=0, errors=0;
};
struct RunSummary {
    long long received=0, emitted=0, aborted=0;
    std::map<std::string,StageMetrics> stages;
    double elapsedMs=0;bool completed=false;
    long long dropped() const {long long n=0;for(auto const& s:stages)n+=s.second.dropped;return n;}
    long long deadLettered() const {long long n=0;for(auto const& s:stages)n+=s.second.deadLettered;return n;}
    bool reconciles() const {return received==emitted+dropped()+deadLettered()+aborted;}
    void print(std::ostream& out) const {
        out << "RunSummary: received=" << received << " emitted=" << emitted << " dropped=" << dropped()
            << " deadLettered=" << deadLettered() << " aborted=" << aborted << " elapsedMs=" << elapsedMs
            << " completed=" << (completed?"true":"false") << " reconciled=" << (reconciles()?"true":"false") << '\n';
        for(auto const& s:stages) out << "  " << s.first << ": dropped=" << s.second.dropped << " deadLettered=" << s.second.deadLettered
            << " failed=" << s.second.failed << " errors=" << s.second.errors << " retries=" << s.second.retries << '\n';
    }
};
struct ErrorPolicy {
    enum Mode {FAIL_FAST, SKIP, DEAD_LETTER, RETRY} mode;
    unsigned retries;
    explicit ErrorPolicy(Mode m=SKIP,unsigned n=0):mode(m),retries(n) {}
    static ErrorPolicy parse(const ConfigValue& c,const std::string& w) {
        if(c.kind==ConfigValue::String) {
            std::string s=c.string(w);
            if(s=="FAIL_FAST")return ErrorPolicy(FAIL_FAST);
            if(s=="SKIP")return ErrorPolicy(SKIP);
            if(s=="DEAD_LETTER")return ErrorPolicy(DEAD_LETTER);
            throw ConfigError(w+": expected FAIL_FAST, SKIP, DEAD_LETTER or {mode:RETRY, retries:n}");
        }
        c.keys({"mode","retries"},w);
        if(c.at("mode",w).string(w+".mode")!="RETRY")throw ConfigError(w+".mode: expected RETRY");
        long long n=c.at("retries",w).integer(w+".retries");
        if(n<0 || n>1000)throw ConfigError(w+".retries: expected 0..1000");
        return ErrorPolicy(RETRY,static_cast<unsigned>(n));
    }
};
struct RunContext {
    RunSummary summary;
    std::unique_ptr<DeadLetterSink> deadLetter;
    std::string rawInput;
    void fail(const std::string& name,const ErrorPolicy& policy,const std::string& message) {
        StageMetrics& m=summary.stages[name];++m.failed;
        if(policy.mode==ErrorPolicy::SKIP){++m.dropped;return;}
        if(policy.mode==ErrorPolicy::DEAD_LETTER) {
            if(!deadLetter) throw PipelineException("dead-letter sink not configured");
            deadLetter->consume(FailedRecord(rawInput,name,message));++m.deadLettered;return;
        }
        throw PipelineException(name+": "+message+(policy.mode==ErrorPolicy::RETRY?" (retry budget exhausted)":" (FAIL_FAST)"));
    }
};
template<typename T> class BufferedEmitter : public Emitter<T> {
public:
    std::vector<T> items;
    void emit(const T& item) override {items.push_back(item);}
};
// Capture only this stage's failures. Forwarding happens outside the catch boundary.
// A failed attempt cannot leak emitted records to the next stage.
template<typename I,typename O> class PolicyStage : public Stage<I,O> {
    Stage<I,O>& stage_;RunContext& ctx_;std::string name_;ErrorPolicy policy_;
public:
    PolicyStage(Stage<I,O>& stage,RunContext& ctx,const std::string& name,ErrorPolicy policy)
        :stage_(stage),ctx_(ctx),name_(name),policy_(policy) {ctx_.summary.stages[name];}
    void open() override {
        try {stage_.open();} catch(const StageException& e){throw PipelineException(name_+": open failed: "+e.what());}
    }
    void close() override {
        try {stage_.close();} catch(const StageException& e){throw PipelineException(name_+": close failed: "+e.what());}
    }
    void process(const I& item,Emitter<O>& next) override {
        for(unsigned attempt=0;;++attempt) {
            BufferedEmitter<O> buffer;
            try {stage_.process(item,buffer);}
            catch(const StageException& e) {
                ++ctx_.summary.stages[name_].errors;
                if(policy_.mode==ErrorPolicy::RETRY && attempt<policy_.retries) {++ctx_.summary.stages[name_].retries;continue;}
                ctx_.fail(name_,policy_,e.what());return;
            }
            if(buffer.items.size()>1)throw PipelineException(name_+": LogFlow record stages must emit at most one record");
            if(buffer.items.empty()) ++ctx_.summary.stages[name_].dropped;
            for(size_t i=0;i<buffer.items.size();++i) next.emit(buffer.items[i]);
            return;
        }
    }
};
class CountingSource : public Source<std::string> {
    Source<std::string>& source_;RunContext& ctx_;
    class Adapter : public Emitter<std::string> {
        Emitter<std::string>& next_;RunContext& ctx_;
    public:
        Adapter(Emitter<std::string>& next,RunContext& ctx):next_(next),ctx_(ctx) {}
        void emit(const std::string& line) override {++ctx_.summary.received;ctx_.rawInput=line;next_.emit(line);}
    };
public:
    CountingSource(Source<std::string>& source,RunContext& ctx):source_(source),ctx_(ctx) {}
    void produce(Emitter<std::string>& next) override {Adapter a(next,ctx_);source_.produce(a);}
};
class CountingSink : public Sink<LogRecord> {
    Sink<LogRecord>& sink_;RunContext& ctx_;
public:
    CountingSink(Sink<LogRecord>& sink,RunContext& ctx):sink_(sink),ctx_(ctx) {}
    void open() override {sink_.open();}
    void close() override {sink_.close();}
    void consume(const LogRecord& r) override {sink_.consume(r);++ctx_.summary.emitted;}
};
// The main pipeline manages its components; this scope manages the independent branch.
template<typename F> void runWithSummary(RunContext& ctx,F flow) {
    auto previous=ctx.summary.stages;
    ctx.summary=RunSummary();
    for(auto const& s:previous)ctx.summary.stages[s.first]=StageMetrics();
    auto start=std::chrono::steady_clock::now();
    auto finish=[&]() {
        ctx.summary.aborted=ctx.summary.received-ctx.summary.emitted-ctx.summary.dropped()-ctx.summary.deadLettered();
        ctx.summary.elapsedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        ctx.summary.print(std::cerr);
        ctx.rawInput.clear();
    };
    bool opened=false;
    try {
        if(ctx.deadLetter){ctx.deadLetter->open();opened=true;}
        flow();
        if(opened){opened=false;ctx.deadLetter->close();}
        ctx.summary.completed=true;
    } catch(...) {
        if(opened)try{ctx.deadLetter->close();}catch(...){}
        finish();throw;
    }
    finish();
}
#endif
