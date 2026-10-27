#ifndef PIPELINE_H
#define PIPELINE_H

#include "Stage.h"
#include "Sink.h"
#include "Emitter.h"
#include <exception>
#include <memory>
struct RunSummary;
#include <functional>
#include <vector>

// Emitter: gelen ogeyi Sink'e verir.
template<typename T>
class SinkEmitterAdapter : public Emitter<T> {
private:
    Sink<T>* sink;
public:
    explicit SinkEmitterAdapter(Sink<T>* s) : sink(s) {}
    void emit(const T& item) override { sink->consume(item); }
};

// Emitter: gelen ogeyi bir Stage'e verir, Stage'in ciktisini bir sonrakine iletir.
template<typename I, typename O>
class StageEmitterAdapter : public Emitter<I> {
private:
    Stage<I, O>* stage;
    Emitter<O>& next;
public:
    StageEmitterAdapter(Stage<I, O>* st, Emitter<O>& n) : stage(st), next(n) {}
    void emit(const I& item) override { stage->process(item, next); }
};

// Bir asamanin (veya sink'in) open()/close() cifti.
struct LifecycleHook {
    std::function<void()> open;
    std::function<void()> close;
};

// Hazir, calistirilabilir bir islem hatti. PipelineBuilder tarafindan uretilir.
//
// Yasam dongusu sirasi (hooks akis sirasiyla gelir: kaynaga yakin -> sink):
//   open  : sink once, kaynaga en yakin asama en son   (hedef hazir olmadan veri akmasin)
//   close : kaynaga en yakin asama once, sink en son   (arabellekteki veri hedefe bosalsin)
// Bir open() hata verirse, o ana kadar acilanlar kapatilir. Akis sirasinda hata
// olursa da close() yine cagrilir (kaynak sizintisi ve veri kaybi olmasin).
class Pipeline {
private:
    std::shared_ptr<RunSummary> summary_;
    std::function<void()> flow_;            // kaynagi calistirir, veriyi zincire akitir
    std::vector<LifecycleHook> hooks_;

    // hooks_[first..son] arasini akis sirasiyla kapatir; ilk hatayi dondurur.
    std::exception_ptr closeFrom(size_t first) {
        std::exception_ptr firstError;
        for (size_t i = first; i < hooks_.size(); ++i) {
            try { hooks_[i].close(); }
            catch (...) { if (!firstError) firstError = std::current_exception(); }
        }
        return firstError;
    }

public:
    Pipeline(const std::function<void()>& flow, const std::vector<LifecycleHook>& hooks, std::shared_ptr<RunSummary> summary = std::shared_ptr<RunSummary>())
        : summary_(summary), flow_(flow), hooks_(hooks) {}

    const RunSummary* summary() const {return summary_.get();}

    void run() {
        size_t firstOpened = hooks_.size();
        try {
            while (firstOpened > 0) {
                hooks_[firstOpened - 1].open();
                --firstOpened;
            }
            flow_();
        } catch (...) {
            closeFrom(firstOpened);         // asil hata korunur; kapatma hatalari yutulur
            throw;
        }
        std::exception_ptr closeError = closeFrom(firstOpened);
        if (closeError) std::rethrow_exception(closeError);
    }
};

#endif
