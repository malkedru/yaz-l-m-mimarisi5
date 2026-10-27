#ifndef STAGE_FACTORY_H
#define STAGE_FACTORY_H
#include "ConfigValue.h"
#include "LogRecord.h"
#include "Stage.h"
#include <functional>
#include <memory>

typedef Stage<LogRecord, LogRecord> ConfigRecordStage;
struct StageInstance {
    std::unique_ptr<Stage<std::string, LogRecord> > parser;
    std::unique_ptr<ConfigRecordStage> record;
    std::vector<std::string> fields;
    std::vector<std::string> requiredFields;
};
typedef std::function<StageInstance(const ConfigValue&, const std::string&)> StageFactory;
#endif
