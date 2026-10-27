#ifndef SOURCE_H
#define SOURCE_H

#include "Emitter.h"
#include "Stage.h"

template<typename O>
class Source {
public:
    virtual ~Source() {}
    virtual void produce(Emitter<O>& out) = 0;
};

#endif
