#ifndef COLLECTINGEMITTER_H
#define COLLECTINGEMITTER_H

#include "../Emitter.h"
#include <vector>

// Test dubloru (test double): yayilan her ogeyi bir listede toplar.
// Dosya sistemine dokunmadan herhangi bir Stage<I,O> test edilebilir;
// her hafta yeniden kullanilacaktir.
template<typename T>
class CollectingEmitter : public Emitter<T> {
public:
    std::vector<T> items;
    void emit(const T& item) override { items.push_back(item); }
};

#endif
