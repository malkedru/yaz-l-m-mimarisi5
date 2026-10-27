#ifndef SINK_H
#define SINK_H

// Veriyi tuketen son halka.
// open()/close() yasam dongusu kancalaridir (Stage ile ayni sozlesme):
// Pipeline, veri akmadan once open(), akis bittikten (veya hata verdikten)
// sonra close() cagirir. Kaynak tutan sink'ler (or. FileSink) bunlari kullanir.
template<typename I>
class Sink {
public:
    virtual ~Sink() {}
    virtual void consume(const I& item) = 0;

    virtual void open() {}
    virtual void close() {}
};

#endif
