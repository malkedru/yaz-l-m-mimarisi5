#ifndef FILESINK_H
#define FILESINK_H

#include "Sink.h"
#include "Stage.h"
#include "LogRecord.h"
#include "ConsoleSink.h"
#include "LogFields.h"
#include <fstream>
#include <string>

// LogRecord'lari bir dosyaya, her kayit bir satir olacak sekilde yazar.
//
// Yasam dongusu:
//   open()    -> dosyayi olusturur (varsa icerigini siler); acilamazsa hata verir.
//   consume() -> bir satir yazar; open() cagrilmadiysa hata verir.
//   close()   -> arabellegi bosaltir, dosyayi kapatir; yazma hatasi varsa bildirir.
//
// close() cagrilmazsa arabellege alinmis son satirlar diske hic ulasmayabilir
// ve yazma hatalari (disk dolu vb.) fark edilmez. Pipeline close()'u her durumda
// cagirir; FileSink'i pipeline disinda kullanan kod bunu kendisi yapmalidir.
class FileSink : public Sink<LogRecord> {
private:
    std::string path_;
    RecordFormatter formatter_;
    std::ofstream out_;
    long long written_;

public:
    explicit FileSink(const std::string& path,
                      const RecordFormatter& formatter = &ConsoleSink::format)
        : path_(path), formatter_(formatter), written_(0) {}

    void open() override {
        out_.open(path_.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out_.is_open())
            throw PipelineException("Cikti dosyasi acilamadi: " + path_);
        written_ = 0;
    }

    void consume(const LogRecord& item) override {
        if (!out_.is_open())
            throw PipelineException("FileSink: open() cagrilmadan yazilamaz: " + path_);
        out_ << formatter_(item) << '\n';
        if (!out_)
            throw PipelineException("Cikti dosyasina yazilamadi: " + path_);
        ++written_;
    }

    void close() override {
        if (!out_.is_open()) return;     // tekrar cagrilmasi zararsizdir
        out_.flush();
        bool ok = out_.good();
        out_.close();
        if (!ok || out_.fail())
            throw PipelineException("Cikti dosyasi duzgun kapatilamadi: " + path_);
    }

    long long writtenCount() const { return written_; }
};

#endif
