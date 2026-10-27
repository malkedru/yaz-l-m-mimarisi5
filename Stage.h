#ifndef STAGE_H
#define STAGE_H

#include "Emitter.h"
#include <stdexcept>
#include <string>

class PipelineException : public std::runtime_error {
public:
    explicit PipelineException(const std::string& message) : std::runtime_error(message) {}
};

class StageException : public std::runtime_error {
public:
    StageException(const std::string& message) : std::runtime_error(message) {}
};

template<typename I, typename O>
class Stage {
public:
    virtual ~Stage() {}

    virtual void process(const I& input, Emitter<O>& out) = 0;

    virtual void open() {}
    virtual void close() {}
};

#endif
