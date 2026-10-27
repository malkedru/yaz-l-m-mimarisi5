#ifndef PROJECTIONSTAGE_H
#define PROJECTIONSTAGE_H

#include "Stage.h"
#include "LogRecord.h"
#include "LogFields.h"
#include <map>
#include <set>
#include <string>
#include <vector>

// Her kaydi secilen alanlarin bir alt kumesine indirger (1 -> 1).
//
// Veri minimizasyonu: yalnizca istenen alanlar tasinir. Secilmeyen alanlar
// bosaltilir (metin: "", sayi: 0). Ham satir (raw) ve genisletme alanlari
// (attributes) her zaman atilir; cunku ham satir, secilmeyen alanlarin
// (or. IP, user agent) tamamini icerir ve projeksiyonu anlamsiz kilardi.
//
// Cikti hâlâ bir LogRecord'dur; bu yuzden asama, hattin herhangi bir yerine
// (filtreden once ya da sonra) yerlestirilebilir. DIKKAT: projeksiyondan
// sonra gelen bir filtre, atilmis alani yalnizca bos/0 degeriyle gorur.
class ProjectionStage : public Stage<LogRecord, LogRecord> {
private:
    std::set<std::string> kept_;
    long long projected_;

    bool keeps(const char* field) const { return kept_.count(field) > 0; }

public:
    explicit ProjectionStage(const std::vector<std::string>& fields) : projected_(0) {
        if (fields.empty())
            throw StageException("ProjectionStage: en az bir alan secilmeli");
        for (size_t i = 0; i < fields.size(); ++i) {
            if (!LogFields::isKnown(fields[i]))
                throw StageException("ProjectionStage: bilinmeyen alan: " + fields[i]);
            kept_.insert(fields[i]);
        }
    }

    void process(const LogRecord& in, Emitter<LogRecord>& out) override {
        ++projected_;
        out.emit(LogRecord(
            keeps("timestamp") ? in.timestamp() : 0,
            keeps("clientIp")  ? in.clientIp()  : std::string(),
            keeps("method")    ? in.method()    : std::string(),
            keeps("path")      ? in.path()      : std::string(),
            keeps("status")    ? in.status()    : 0,
            keeps("bytes")     ? in.bytes()     : 0LL,
            keeps("userAgent") ? in.userAgent() : std::string(),
            std::map<std::string, std::string>(),
            std::string()));
    }

    long long projectedCount() const { return projected_; }
};

#endif
