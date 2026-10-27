#ifndef TESTHARNESS_H
#define TESTHARNESS_H

// Cok kucuk test iskeleti (dis kutuphane yok; Dev-C++ ile de derlenir).
// Her test dosyasi kendi main()'ini yazar: run(...) cagrilari + return finish();
#include <iostream>

static int failures = 0, total = 0;

#define CHECK(cond) do { ++total; if (!(cond)) { ++failures; \
    std::cerr << "  BASARISIZ: " #cond " (satir " << __LINE__ << ")" << std::endl; } } while (0)

// Ifade verilen istisna tipini FIRLATMALI.
#define CHECK_THROWS(expr, ExceptionType) do { ++total; bool caught = false; \
    try { expr; } catch (const ExceptionType&) { caught = true; } \
    if (!caught) { ++failures; \
    std::cerr << "  BASARISIZ: " #expr " istisna firlatmadi (satir " << __LINE__ << ")" << std::endl; } \
    } while (0)

static void run(const char* name, void (*fn)()) {
    int before = failures;
    fn();
    std::cout << (failures == before ? "[GECTI]  " : "[KALDI]  ") << name << std::endl;
}

static int finish() {
    std::cout << "\n" << (total - failures) << "/" << total << " dogrulama gecti." << std::endl;
    return failures == 0 ? 0 : 1;
}

#endif
