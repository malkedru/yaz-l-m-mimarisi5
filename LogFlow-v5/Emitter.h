#ifndef EMITTER_H
#define EMITTER_H

template<typename T>
class Emitter {
public:
    virtual ~Emitter() {}
    virtual void emit(const T& item) = 0;
};

#endif
