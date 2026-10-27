#ifndef STAGE_REGISTRY_H
#define STAGE_REGISTRY_H
#include "ConfigValue.h"
#include "StageFactory.h"
#include "ParserStage.h"
#include "PredicateFilterStage.h"
#include "ProjectionStage.h"
#include "LogFields.h"
#include "ValidationStage.h"
#include <memory>
#include <functional>
#include <set>

class StageRegistry {
    std::map<std::string, StageFactory> factories_;
public:
    void add(const std::string& name, const StageFactory& factory) {
        if(name.empty() || !factory || !factories_.insert(std::make_pair(name,factory)).second)
            throw ConfigError("registry: invalid or duplicate stage registration: " + name);
    }
    StageInstance create(const std::string& name, const ConfigValue& params, const std::string& where) const {
        std::map<std::string,StageFactory>::const_iterator it=factories_.find(name);
        if(it==factories_.end()) {
            std::string names;
            for(it=factories_.begin();it!=factories_.end();++it) names+=(names.empty()?"":", ")+it->first;
            throw ConfigError(where+".type: unknown stage '"+name+"'; registered: "+names);
        }
        return it->second(params,where+".params");
    }
};

namespace BuiltinStages {
inline StageInstance parse(const ConfigValue& p, const std::string& w) {
    p.keys({"format","maxLineBytes"},w);
    if(p.at("format",w).string(w+".format")!="clf") throw ConfigError(w+".format: supported format is clf");
    long long limit=65536;
    if(p.object.count("maxLineBytes"))limit=p.at("maxLineBytes",w).integer(w+".maxLineBytes");
    if(limit<1 || limit>10485760)throw ConfigError(w+".maxLineBytes: expected 1..10485760");
    StageInstance s;s.parser.reset(new ParserStage(static_cast<size_t>(limit)));return s;
}
inline StageInstance project(const ConfigValue& p, const std::string& w) {
    p.keys({"fields"},w);const ConfigValue& a=p.at("fields",w);
    if(a.kind!=ConfigValue::Array || a.array.empty()) throw ConfigError(w+".fields: non-empty array required");
    StageInstance s;std::set<std::string> seen;
    for(size_t i=0;i<a.array.size();++i) {
        std::string f=a.array[i].string(w+".fields");
        if(!LogFields::isKnown(f) || !seen.insert(f).second) throw ConfigError(w+".fields: unknown or duplicate field '"+f+"'");
        s.fields.push_back(f);
    }
    s.record.reset(new ProjectionStage(s.fields));return s;
}
inline StageInstance filter(const ConfigValue& p, const std::string& w) {
    p.keys({"field","op","value"},w);
    std::string field=p.at("field",w).string(w+".field"), op=p.at("op",w).string(w+".op");
    if(!LogFields::isKnown(field)) throw ConfigError(w+".field: unknown field '"+field+"'");
    const ConfigValue& v=p.at("value",w);
    bool numeric=field=="status" || field=="bytes";
    if(op!="==" && op!="!=" && op!=">=" && op!="<=" && op!=">" && op!="<" && op!="startsWith")
        throw ConfigError(w+".op: expected ==, !=, >=, <=, >, <, startsWith");
    if(numeric && op=="startsWith") throw ConfigError(w+".op: startsWith requires a text field");
    long long number=0;std::string text;
    if(numeric) {
        number=v.integer(w+".value");
        if((field=="status" && (number<100 || number>599)) || (field=="bytes" && number<0))
            throw ConfigError(w+".value: status must be 100..599; bytes must be non-negative");
    } else text=v.string(w+".value");
    StageInstance s;
    s.requiredFields.push_back(field);
    s.record.reset(new PredicateFilterStage([field,op,numeric,number,text](const LogRecord& r) {
        int cmp;
        if(numeric) {long long n=field=="status"?r.status():r.bytes();cmp=n<number?-1:(n>number?1:0);}
        else {std::string x=LogFields::valueOf(r,field);if(op=="startsWith") return x.compare(0,text.size(),text)==0;cmp=x.compare(text);}
        if(op=="==") return cmp==0;
        if(op=="!=") return cmp!=0;
        if(op==">=") return cmp>=0;
        if(op=="<=") return cmp<=0;
        if(op==">") return cmp>0;
        return cmp<0;
    }));return s;
}
inline StageInstance validate(const ConfigValue& p,const std::string& w) {
    StageInstance s;
    std::unique_ptr<ValidationStage> v(new ValidationStage(p,w));
    s.requiredFields=v->requiredFields();s.record=std::move(v);return s;
}
inline StageRegistry registry() {
    // The single registration table: extend here, never add a loader switch.
    const std::pair<const char*, StageFactory> table[]={
        {"validate", &validate}, {"parse", &parse}, {"filter", &filter}, {"project", &project}
    };
    StageRegistry r;
    for(size_t i=0;i<sizeof(table)/sizeof(table[0]);++i) r.add(table[i].first,table[i].second);
    return r;
}
}
#endif
