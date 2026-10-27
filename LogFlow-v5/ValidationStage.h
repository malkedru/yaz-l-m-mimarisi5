#ifndef VALIDATION_STAGE_H
#define VALIDATION_STAGE_H
#include "Stage.h"
#include "LogRecord.h"
#include "LogFields.h"
#include "ConfigValue.h"
#include <set>
#include <limits>

class ValidationStage : public Stage<LogRecord,LogRecord> {
    std::vector<std::string> required_;
    std::map<std::string,std::pair<long long,long long> > ranges_;
    bool ip_=false;
    long long minTime_=std::numeric_limits<long long>::min(), maxTime_=std::numeric_limits<long long>::max();
    static bool ipv4(const std::string& s) {
        size_t start=0;
        for(int part=0;part<4;++part) {
            size_t end=s.find('.',start);if(end==std::string::npos)end=s.size();
            if(end==start || end-start>3 || (end-start>1 && s[start]=='0'))return false;
            unsigned n=0;for(size_t i=start;i<end;++i){if(s[i]<'0'||s[i]>'9')return false;n=n*10+(s[i]-'0');}
            if(n>255 || (part<3 && end==s.size()) || (part==3 && end!=s.size()))return false;
            start=end+1;
        }return true;
    }
    static bool ipv6(const std::string& s) {
        size_t compressed=s.find("::");
        if(compressed!=std::string::npos && s.find("::",compressed+2)!=std::string::npos)return false;
        int groups=0;size_t p=0;
        while(p<s.size()) {
            if(s[p]==':'){++p;continue;}
            size_t e=s.find(':',p);if(e==std::string::npos)e=s.size();
            std::string token=s.substr(p,e-p);
            if(token.find('.')!=std::string::npos){if(e!=s.size() || !ipv4(token))return false;groups+=2;}
            else {if(token.size()>4)return false;for(char c:token)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;++groups;}
            p=e;
        }
        if(s.empty() || (s.front()==':' && s.compare(0,2,"::")!=0) || (s.back()==':' && (s.size()<2 || s.substr(s.size()-2)!="::")))return false;
        if(s.find(":::")!=std::string::npos)return false;
        return compressed==std::string::npos ? groups==8 : groups<8;
    }
public:
    static bool validIp(const std::string& ip){return ip.find(':')==std::string::npos?ipv4(ip):ipv6(ip);}
    static std::pair<long long,long long> bounds(const ConfigValue& p,const std::string& w) {
        p.keys({"min","max"},w);
        long long lo=p.at("min",w).integer(w+".min"),hi=p.at("max",w).integer(w+".max");
        if(lo>hi)throw ConfigError(w+": min must not exceed max");return std::make_pair(lo,hi);
    }
    explicit ValidationStage(const ConfigValue& p,const std::string& w) {
        p.keys({"required","ranges","timestamp","ip"},w);
        if(p.object.empty())throw ConfigError(w+": at least one validation rule required");
        if(p.object.count("required")) {
            const ConfigValue& a=p.at("required",w);
            if(a.kind!=ConfigValue::Array || a.array.empty())throw ConfigError(w+".required: non-empty array required");
            std::set<std::string> seen;
            for(auto const& v:a.array){std::string f=v.string(w+".required");if(!LogFields::isKnown(f)||!seen.insert(f).second)throw ConfigError(w+".required: unknown or duplicate field");required_.push_back(f);}
        }
        if(p.object.count("ranges")) {
            const ConfigValue& r=p.at("ranges",w);r.keys({"status","bytes"},w+".ranges");
            if(r.object.empty())throw ConfigError(w+".ranges: at least one range required");
            for(auto const& v:r.object)ranges_[v.first]=bounds(v.second,w+".ranges."+v.first);
        }
        if(p.object.count("timestamp")){auto b=bounds(p.at("timestamp",w),w+".timestamp");minTime_=b.first;maxTime_=b.second;}
        if(p.object.count("ip")) {
            const ConfigValue& v=p.at("ip",w);
            if(v.kind!=ConfigValue::Boolean)throw ConfigError(w+".ip: boolean required");ip_=v.text=="true";
        }
    }
    std::vector<std::string> requiredFields() const {
        std::vector<std::string> out=required_;for(auto const& r:ranges_)out.push_back(r.first);
        if(ip_)out.push_back("clientIp");
        if(minTime_!=std::numeric_limits<long long>::min()||maxTime_!=std::numeric_limits<long long>::max())out.push_back("timestamp");
        return out;
    }
    void process(const LogRecord& r,Emitter<LogRecord>& out) override {
        for(auto const& f:required_) {
            bool missing=LogFields::valueOf(r,f).empty();
            if(f=="timestamp")missing=r.timestamp()==0;
            if(f=="status")missing=r.status()==0;
            if(missing)throw StageException("required field missing: "+f);
        }
        for(auto const& v:ranges_) {
            long long n=v.first=="status"?r.status():r.bytes();
            if(n<v.second.first||n>v.second.second)throw StageException("numeric range violation: "+v.first);
        }
        if(static_cast<long long>(r.timestamp())<minTime_||static_cast<long long>(r.timestamp())>maxTime_)throw StageException("timestamp outside configured sanity range");
        if(ip_ && !validIp(r.clientIp()))throw StageException("invalid IP address: "+r.clientIp());
        out.emit(r);
    }
};
#endif
