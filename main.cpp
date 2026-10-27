

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "CommandLine.h"
#include "ConsoleSink.h"
#include "FileLineSource.h"
#include "FileSink.h"
#include "ParserStage.h"
#include "Pipeline.h"
#include "PipelineBuilder.h"
#include "PredicateFilterStage.h"
#include "ProjectionStage.h"
#include "PipelineLoader.h"

typedef Stage<LogRecord, LogRecord> RecordStage;

// Komut satiri ayarlarina gore filtre ve projeksiyon asamalarini uretir.
// Sira: once filtreler (ucuz ve eleyici), projeksiyon en sonda.
// Verilmeyen ayar icin asama hic olusturulmaz.
static std::vector<std::unique_ptr<RecordStage> > buildRecordStages(const Options& opt) {
    std::vector<std::unique_ptr<RecordStage> > stages;
    if (opt.hasMinStatus)
        stages.push_back(std::unique_ptr<RecordStage>(
            new PredicateFilterStage(Predicates::statusAtLeast(opt.minStatus))));
    if (opt.hasPathPrefix)
        stages.push_back(std::unique_ptr<RecordStage>(
            new PredicateFilterStage(Predicates::pathStartsWith(opt.pathPrefix))));
    if (!opt.fields.empty())
        stages.push_back(std::unique_ptr<RecordStage>(new ProjectionStage(opt.fields)));
    return stages;
}

// --output verilmisse FileSink, verilmemisse ConsoleSink.
static std::unique_ptr<Sink<LogRecord> > buildSink(const Options& opt) {
    RecordFormatter format = opt.fields.empty()
        ? RecordFormatter(&ConsoleSink::format)
        : LogFields::tabSeparated(opt.fields);

    if (opt.output.empty())
        return std::unique_ptr<Sink<LogRecord> >(new ConsoleSink(format));
    return std::unique_ptr<Sink<LogRecord> >(new FileSink(opt.output, format));
}

static int runLogFilter(const Options& opt) {
    FileLineSource source(opt.input);
    ParserStage parser;
    std::vector<std::unique_ptr<RecordStage> > stages = buildRecordStages(opt);
    std::unique_ptr<Sink<LogRecord> > sink = buildSink(opt);

    RunContext context;
    CountingSource countedSource(source,context);
    CountingSink countedSink(*sink,context);
    PolicyStage<std::string,LogRecord> guardedParser(parser,context,"parse",ErrorPolicy(ErrorPolicy::SKIP));
    std::vector<std::unique_ptr<PolicyStage<LogRecord,LogRecord> > > guarded;
    PipelineStep<LogRecord> chain = PipelineBuilder::from(countedSource).via(guardedParser);
    for (size_t i=0;i<stages.size();++i) {
        guarded.push_back(std::unique_ptr<PolicyStage<LogRecord,LogRecord> >(new PolicyStage<LogRecord,LogRecord>(*stages[i],context,"stage["+std::to_string(i)+"]",ErrorPolicy())));
        chain=chain.via(*guarded.back());
    }
    Pipeline pipeline=chain.to(countedSink);
    runWithSummary(context,[&pipeline](){pipeline.run();});

    return 0;
}

int main(int argc, char* argv[]) {
    try {
        Options opt = CommandLine::parse(argc, argv);
        if (opt.help) {
            std::cout << CommandLine::usage();
            return 0;
        }
        if (opt.version) {
            std::cout << "LogFlow v5" << std::endl;
            return 0;
        }
        if (!opt.config.empty()) {
            Pipeline pipeline = PipelineLoader().load(opt.config);
            pipeline.run();
            return 0;
        }
        return runLogFilter(opt);

    } catch (const ConfigError& e) {
        std::cerr << "Yapilandirma hatasi: " << e.what() << std::endl;
        return 2;
    } catch (const UsageError& e) {
        std::cerr << "Kullanim hatasi: " << e.what() << std::endl << std::endl
                  << CommandLine::usage();
        return 2;
    } catch (const PipelineException& e) {
        std::cerr << "Pipeline hatasi: " << e.what() << std::endl;
        return 1;
    } catch (const StageException& e) {
        std::cerr << "Calisma hatasi: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Beklenmeyen hata: " << e.what() << std::endl;
        return 1;
    }
}
