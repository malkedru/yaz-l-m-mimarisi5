#ifndef PREDICATES_H
#define PREDICATES_H

#include "LogRecord.h"
#include <ctime>
#include <functional>
#include <string>

// Bir kaydin kosulu saglayip saglamadigini soyleyen islev (Java'daki Predicate<LogRecord>).
typedef std::function<bool(const LogRecord&)> LogPredicate;

// PredicateFilterStage ile kullanilan hazir kosullar.
// Her biri ayri ve kucuk bir islevdir; yeni kosul eklemek asamayi degistirmeyi gerektirmez.
namespace Predicates {

// status >= alt sinir
inline LogPredicate statusAtLeast(int minStatus) {
    return [minStatus](const LogRecord& r) { return r.status() >= minStatus; };
}

// path, verilen onekle basliyor mu?
inline LogPredicate pathStartsWith(const std::string& prefix) {
    return [prefix](const LogRecord& r) {
        return r.path().compare(0, prefix.size(), prefix) == 0;
    };
}

// from <= zaman damgasi <= to (iki uc dahil, UTC epoch saniyesi)
inline LogPredicate timestampBetween(std::time_t from, std::time_t to) {
    return [from, to](const LogRecord& r) {
        return r.timestamp() >= from && r.timestamp() <= to;
    };
}

}  // namespace Predicates

#endif
