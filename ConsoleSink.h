#ifndef CONSOLESINK_H
#define CONSOLESINK_H

#include "Sink.h"
#include "LogRecord.h"
#include "LogFields.h"
#include <iostream>
#include <sstream>
#include <string>

// LogRecord'u ekrana yazar. Varsayilan bicim okunabilir TEK SATIRDIR:
// 2024-01-01T10:15:30Z 192.168.1.1 GET /index.html -> 200 (1234 B)
// Istenirse baska bir bicimleyici verilebilir (or. --fields ile secilen alanlar).
class ConsoleSink : public Sink<LogRecord> {
private:
    RecordFormatter formatter_;

public:
    ConsoleSink() : formatter_(&ConsoleSink::format) {}
    explicit ConsoleSink(const RecordFormatter& formatter) : formatter_(formatter) {}

    static std::string format(const LogRecord& r) {
        std::ostringstream os;
        os << LogFields::formatTimestamp(r.timestamp()) << " " << r.clientIp()
           << " " << r.method() << " " << r.path()
           << " -> " << r.status() << " (" << r.bytes() << " B)";
        if (!r.userAgent().empty()) os << " UA=\"" << r.userAgent() << "\"";
        return os.str();
    }

    void consume(const LogRecord& item) override {
        std::cout << formatter_(item) << std::endl;
        if(!std::cout) throw PipelineException("console write failed");
    }
};

#endif
