#ifndef CONFIG_VALUE_H
#define CONFIG_VALUE_H
#include "Stage.h"
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <cstdlib>

class ConfigError : public PipelineException {
public: explicit ConfigError(const std::string& s) : PipelineException(s) {}
};

// Strict JSON: typed values, duplicate-key rejection, bounded nesting and location errors.
struct ConfigValue {
    enum Kind { Null, Boolean, Number, String, Array, Object } kind;
    std::string text;
    std::vector<ConfigValue> array;
    std::map<std::string, ConfigValue> object;
    ConfigValue(Kind k = Null) : kind(k) {}
    const ConfigValue& at(const std::string& key, const std::string& where) const {
        if (kind != Object) throw ConfigError(where + ": object required");
        std::map<std::string, ConfigValue>::const_iterator it = object.find(key);
        if (it == object.end()) throw ConfigError(where + "." + key + ": required parameter missing");
        return it->second;
    }
    std::string string(const std::string& where) const {
        if (kind != String || text.empty()) throw ConfigError(where + ": non-empty string required");
        return text;
    }
    long long integer(const std::string& where) const {
        if (kind != Number || text.find_first_of(".eE") != std::string::npos)
            throw ConfigError(where + ": integer required");
        std::istringstream in(text); long long n; in >> n;
        if (in.fail()) throw ConfigError(where + ": integer out of range");
        return n;
    }
    void keys(const std::vector<std::string>& allowed, const std::string& where) const {
        if (kind != Object) throw ConfigError(where + ": object required");
        for (std::map<std::string, ConfigValue>::const_iterator i=object.begin(); i!=object.end(); ++i) {
            bool found=false;
            for (size_t j=0;j<allowed.size();++j) if(i->first==allowed[j]) found=true;
            if (!found) throw ConfigError(where + "." + i->first + ": unknown parameter");
        }
    }
};

class ConfigParser {
    std::string s; size_t pos;
    void fail(const std::string& message) const {
        size_t line=1, col=1;
        for(size_t i=0;i<pos && i<s.size();++i) { if(s[i]=='\n') {++line;col=1;} else ++col; }
        std::ostringstream out; out << "JSON line " << line << ", column " << col << ": " << message;
        throw ConfigError(out.str());
    }
    void ws() { while(pos<s.size() && (s[pos]==' ' || s[pos]=='\n' || s[pos]=='\r' || s[pos]=='\t')) ++pos; }
    bool take(char c) { ws(); if(pos<s.size() && s[pos]==c) {++pos;return true;} return false; }
    unsigned hex4() {
        unsigned n=0;
        for(int i=0;i<4;++i) {
            if(pos>=s.size()) fail("incomplete unicode escape");
            char c=s[pos++]; n*=16;
            if(c>='0'&&c<='9') n+=c-'0'; else if(c>='a'&&c<='f') n+=c-'a'+10;
            else if(c>='A'&&c<='F') n+=c-'A'+10; else fail("invalid unicode escape");
        } return n;
    }
    static void utf8(std::string& out, unsigned n) {
        if(n<128) out+=static_cast<char>(n);
        else if(n<2048) {out+=static_cast<char>(192|(n>>6));out+=static_cast<char>(128|(n&63));}
        else if(n<65536) {out+=static_cast<char>(224|(n>>12));out+=static_cast<char>(128|((n>>6)&63));out+=static_cast<char>(128|(n&63));}
        else {out+=static_cast<char>(240|(n>>18));out+=static_cast<char>(128|((n>>12)&63));out+=static_cast<char>(128|((n>>6)&63));out+=static_cast<char>(128|(n&63));}
    }
    std::string quoted() {
        if(!take('"')) fail("string expected");
        std::string out;
        while(pos<s.size()) {
            unsigned char c=static_cast<unsigned char>(s[pos++]);
            if(c=='"') return out;
            if(c<32) fail("control character in string");
            if(c!='\\') {out+=static_cast<char>(c);continue;}
            if(pos>=s.size()) fail("incomplete escape");
            char e=s[pos++];
            if(e=='"'||e=='\\'||e=='/') out+=e;
            else if(e=='b') out+='\b'; else if(e=='f') out+='\f';
            else if(e=='n') out+='\n'; else if(e=='r') out+='\r'; else if(e=='t') out+='\t';
            else if(e=='u') {
                unsigned n=hex4();
                if(n>=0xD800 && n<=0xDBFF) {
                    if(pos+2>s.size() || s[pos]!='\\' || s[pos+1]!='u') fail("missing low surrogate");
                    pos+=2; unsigned low=hex4();
                    if(low<0xDC00 || low>0xDFFF) fail("invalid low surrogate");
                    n=0x10000+((n-0xD800)<<10)+(low-0xDC00);
                } else if(n>=0xDC00 && n<=0xDFFF) fail("unexpected low surrogate");
                utf8(out,n);
            } else fail("invalid escape");
        } fail("unterminated string"); return out;
    }
    ConfigValue value(unsigned depth) {
        if(depth>64) fail("maximum nesting depth exceeded");
        ws(); if(pos>=s.size()) fail("value expected");
        if(s[pos]=='{') {
            ++pos; ConfigValue v(ConfigValue::Object); if(take('}')) return v;
            do {std::string key=quoted(); if(!take(':')) fail("':' expected");
                ConfigValue item=value(depth+1);
                if(!v.object.insert(std::make_pair(key,item)).second) fail("duplicate key: "+key);
            } while(take(','));
            if(!take('}')) fail("'}' expected"); return v;
        }
        if(s[pos]=='[') {
            ++pos; ConfigValue v(ConfigValue::Array); if(take(']')) return v;
            do {v.array.push_back(value(depth+1));} while(take(','));
            if(!take(']')) fail("']' expected"); return v;
        }
        if(s[pos]=='"') {ConfigValue v(ConfigValue::String);v.text=quoted();return v;}
        const char* literals[]={"true","false","null"};
        for(int i=0;i<3;++i) if(s.compare(pos,std::string(literals[i]).size(),literals[i])==0) {
            ConfigValue v(i==2?ConfigValue::Null:ConfigValue::Boolean);v.text=literals[i];pos+=v.text.size();return v;
        }
        size_t start=pos; if(s[pos]=='-') ++pos;
        if(pos>=s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) fail("value expected");
        if(s[pos]=='0') ++pos; else while(pos<s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        if(pos<s.size() && s[pos]=='.') {++pos;size_t b=pos;while(pos<s.size()&&std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;if(b==pos) fail("fraction expected");}
        if(pos<s.size() && (s[pos]=='e'||s[pos]=='E')) {++pos;if(pos<s.size()&&(s[pos]=='+'||s[pos]=='-')) ++pos;size_t b=pos;while(pos<s.size()&&std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;if(b==pos) fail("exponent expected");}
        ConfigValue v(ConfigValue::Number);v.text=s.substr(start,pos-start);return v;
    }
public:
    explicit ConfigParser(const std::string& text):s(text),pos(0) {}
    ConfigValue parse() { if(s.compare(0,3,"\xEF\xBB\xBF")==0) pos=3; ConfigValue v=value(0);ws();if(pos!=s.size()) fail("unexpected trailing content");return v; }
};
#endif
