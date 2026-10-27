#ifndef TESTSUPPORT_H
#define TESTSUPPORT_H

// Birden fazla test dosyasinin paylastigi test yardimcilari (dublorler).
#include "../Emitter.h"
#include "../Source.h"
#include "../Stage.h"
#include "../Sink.h"
#include "../LogRecord.h"
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Kisa yoldan LogRecord uretir.
inline LogRecord makeRecord(std::time_t ts, const std::string& ip,
                            const std::string& path, int status,
                            long long bytes = 0) {
    return LogRecord(ts, ip, "GET", path, status, bytes, "test-agent",
                     std::map<std::string, std::string>(), "ham satir: " + path);
}

// Bellekteki bir listeyi kaynak olarak akitir (dosya gerekmez).
template<typename T>
class ListSource : public Source<T> {
public:
    std::vector<T> items;
    void produce(Emitter<T>& out) override {
        for (size_t i = 0; i < items.size(); ++i) out.emit(items[i]);
    }
};

// Tuketilen her ogeyi toplayan sink.
template<typename T>
class CollectingSink : public Sink<T> {
public:
    std::vector<T> items;
    void consume(const T& item) override { items.push_back(item); }
};

// Yasam dongusu olaylarini ortak bir listeye yazan asama ("open:ad", "close:ad").
// failOnOpen=true ise open() hata verir (basarisiz acilisi denemek icin).
template<typename T>
class TraceStage : public Stage<T, T> {
private:
    std::string name_;
    std::vector<std::string>& log_;
    bool failOnOpen_;
public:
    TraceStage(const std::string& name, std::vector<std::string>& log, bool failOnOpen = false)
        : name_(name), log_(log), failOnOpen_(failOnOpen) {}
    void open() override {
        if (failOnOpen_) throw StageException("open basarisiz: " + name_);
        log_.push_back("open:" + name_);
    }
    void close() override { log_.push_back("close:" + name_); }
    void process(const T& in, Emitter<T>& out) override { out.emit(in); }
};

// Yasam dongusunu kaydeden sink; failOnConsume=true ise consume() hata verir.
template<typename T>
class TraceSink : public Sink<T> {
private:
    std::vector<std::string>& log_;
    bool failOnConsume_;
public:
    TraceSink(std::vector<std::string>& log, bool failOnConsume = false)
        : log_(log), failOnConsume_(failOnConsume) {}
    void open() override { log_.push_back("open:sink"); }
    void close() override { log_.push_back("close:sink"); }
    void consume(const T&) override {
        if (failOnConsume_) throw StageException("consume basarisiz");
    }
};

// Bir dosyanin tum icerigini okur (yoksa bos dizgi).
inline std::string readFile(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

#endif
