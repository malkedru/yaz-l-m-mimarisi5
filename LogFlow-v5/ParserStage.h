#ifndef PARSERSTAGE_H
#define PARSERSTAGE_H

#include "Stage.h"
#include "LogRecord.h"
#include <cstring>
#include <memory>
#include <map>
#include <string>

// Ortak Gunluk Bicimi (CLF / Combined) ayristirici asamasi: string -> LogRecord
//
//   IP kimlik kullanici [gg/Aaa/yyyy:SS:dd:ss +zzzz] "METODA YOL PROTOKOL" durum bayt ["referer" "user-agent"]
//
// Strict mode reports record failures to the surrounding policy boundary.
class ParserStage : public Stage<std::string, LogRecord> {
private:
    long long parsedCount_;
    long long badCount_;
    size_t maxLineBytes_;

    static bool isDigits(const std::string& s) {
        if (s.empty()) return false;
        for (size_t i = 0; i < s.size(); ++i)
            if (s[i] < '0' || s[i] > '9') return false;
        return true;
    }

    static bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    static void skipSpaces(const std::string& s, size_t& p) {
        while (p < s.size() && isSpace(s[p])) ++p;
    }

    // Bosluga kadar bir sozcuk (token) okur.
    static bool readToken(const std::string& s, size_t& p, std::string& tok) {
        skipSpaces(s, p);
        size_t start = p;
        while (p < s.size() && !isSpace(s[p])) ++p;
        tok = s.substr(start, p - start);
        return !tok.empty();
    }

    // Acilis karakterinden (open) kapanisina (close) kadar okur;
    // tirnak icindeki bosluklar korunur.
    static bool readDelimited(const std::string& s, size_t& p, char open,
                              char close, std::string& out) {
        skipSpaces(s, p);
        if (p >= s.size() || s[p] != open) return false;
        size_t end = s.find(close, p + 1);
        if (end == std::string::npos) return false;
        out = s.substr(p + 1, end - p - 1);
        p = end + 1;
        return true;
    }

    static bool isLeap(int y) {
        return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    }

    // Gregoryen takvim -> 1970-01-01'den beri gun sayisi (tasinabilir; timegm gerekmez).
    static long long daysFromCivil(int y, int m, int d) {
        y -= m <= 2;
        long long era = (y >= 0 ? y : y - 399) / 400;
        long long yoe = y - era * 400;
        long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    static int toInt(const std::string& s) {
        int v = 0;
        for (size_t i = 0; i < s.size(); ++i) v = v * 10 + (s[i] - '0');
        return v;
    }

    // "01/Jan/2024:10:15:30 +0000" -> UTC epoch saniyesi
    static bool parseTimestamp(const std::string& t, std::time_t& result) {
        // dd/Mon/yyyy:HH:MM:SS +zzzz  => tam 26 karakter
        if (t.size() != 26) return false;
        if (t[2] != '/' || t[6] != '/' || t[11] != ':' || t[14] != ':' ||
            t[17] != ':' || t[20] != ' ')
            return false;

        std::string dd = t.substr(0, 2), mon = t.substr(3, 3),
                    yyyy = t.substr(7, 4), hh = t.substr(12, 2),
                    mi = t.substr(15, 2), ss = t.substr(18, 2),
                    oh = t.substr(22, 2), om = t.substr(24, 2);
        if (!isDigits(dd) || !isDigits(yyyy) || !isDigits(hh) || !isDigits(mi) ||
            !isDigits(ss) || !isDigits(oh) || !isDigits(om))
            return false;
        char sign = t[21];
        if (sign != '+' && sign != '-') return false;

        static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                       "Jul","Aug","Sep","Oct","Nov","Dec"};
        int m = 0;
        for (int i = 0; i < 12; ++i)
            if (mon == months[i]) { m = i + 1; break; }
        if (m == 0) return false;

        int d = toInt(dd), y = toInt(yyyy), H = toInt(hh), M = toInt(mi), S = toInt(ss);
        int offH = toInt(oh), offM = toInt(om);
        static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int maxDay = mdays[m - 1] + ((m == 2 && isLeap(y)) ? 1 : 0);
        if (d < 1 || d > maxDay || H > 23 || M > 59 || S > 59 || offH > 23 || offM > 59)
            return false;

        long long secs = daysFromCivil(y, m, d) * 86400LL + H * 3600LL + M * 60LL + S;
        long long off = offH * 3600LL + offM * 60LL;
        secs -= (sign == '+') ? off : -off;   // yerel saat -> UTC
        result = static_cast<std::time_t>(secs);
        return true;
    }

public:
    explicit ParserStage(size_t maxLineBytes = 65536)
        : parsedCount_(0), badCount_(0), maxLineBytes_(maxLineBytes) {}

    static bool validUtf8(const std::string& s) {
        for (size_t i=0;i<s.size();) {
            unsigned char c=static_cast<unsigned char>(s[i++]);
            if(c<128) continue;
            unsigned n=0, value=0, minimum=0;
            if(c>=0xC2 && c<=0xDF) {n=1;value=c&31;minimum=128;}
            else if(c>=0xE0 && c<=0xEF) {n=2;value=c&15;minimum=2048;}
            else if(c>=0xF0 && c<=0xF4) {n=3;value=c&7;minimum=65536;}
            else return false;
            if(i+n>s.size()) return false;
            while(n--) {unsigned char b=static_cast<unsigned char>(s[i++]);if((b&0xC0)!=0x80) return false;value=(value<<6)|(b&63);}
            if(value<minimum || value>0x10FFFF || (value>=0xD800 && value<=0xDFFF)) return false;
        }
        return true;
    }

    long long parsedCount() const { return parsedCount_; }
    long long badCount() const { return badCount_; }

    // Yalnizca ayristirir; Emitter/sayac icermez (test edilebilirlik icin herkese acik).
    static bool parseLine(const std::string& line, LogRecord*& record) {
        record = 0;
        size_t p = 0;
        std::string ip, ident, user;
        if (!readToken(line, p, ip)) return false;
        if (!readToken(line, p, ident)) return false;
        if (!readToken(line, p, user)) return false;

        std::string ts;
        if (!readDelimited(line, p, '[', ']', ts)) return false;
        std::time_t when = 0;
        if (!parseTimestamp(ts, when)) return false;

        std::string request;
        if (!readDelimited(line, p, '"', '"', request)) return false;
        size_t rp = 0;
        std::string method, path, proto;
        if (!readToken(request, rp, method)) return false;
        if (!readToken(request, rp, path)) return false;
        readToken(request, rp, proto);   // protokol istege bagli

        std::string statusTok, bytesTok;
        if (!readToken(line, p, statusTok)) return false;
        if (!isDigits(statusTok) || statusTok.size() != 3) return false;
        int status = toInt(statusTok);
        if (status < 100 || status > 599) return false;

        if (!readToken(line, p, bytesTok)) return false;
        long long bytes = 0;
        if (bytesTok != "-") {
            if (!isDigits(bytesTok) || bytesTok.size() > 18) return false;
            for (size_t i = 0; i < bytesTok.size(); ++i)
                bytes = bytes * 10 + (bytesTok[i] - '0');
        }

        // Combined bicimi: istege bagli "referer" "user-agent"
        std::string referer, userAgent;
        skipSpaces(line,p);
        if (p<line.size()) {
            if (!readDelimited(line,p,'"','"',referer)) return false;
            if (!readDelimited(line,p,'"','"',userAgent)) return false;
            skipSpaces(line,p);
            if(p!=line.size())return false;
        }
        record = new LogRecord(when, ip, method, path, status, bytes, userAgent,
                               std::map<std::string, std::string>(), line);
        return true;
    }

    void process(const std::string& input, Emitter<LogRecord>& out) override {
        if(input.size()>maxLineBytes_ || !validUtf8(input)) {
            ++badCount_;
            throw StageException(input.size()>maxLineBytes_ ? "line exceeds maxLineBytes" : "invalid UTF-8 input");
        }
        LogRecord* rec = 0;
        if (!parseLine(input, rec)) {
            ++badCount_;
            throw StageException("malformed CLF record (timestamp/request/status/bytes)");
        }
        ++parsedCount_;
        std::unique_ptr<LogRecord> guard(rec);
        out.emit(*rec);
    }
};

#endif
