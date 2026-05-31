#pragma once

#include <stdint.h>

namespace client::ui {

inline uint64_t callback_deps_mix(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    return seed;
}

inline uint64_t callback_deps(uint64_t a, uint64_t b = 0, uint64_t c = 0,
                              uint64_t d = 0) {
    uint64_t seed = 0xcbf29ce484222325ull;
    seed = callback_deps_mix(seed, a);
    seed = callback_deps_mix(seed, b);
    seed = callback_deps_mix(seed, c);
    seed = callback_deps_mix(seed, d);
    return seed;
}

template <typename T>
inline uint64_t callback_deps_ptr(T *ptr) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
}

} // namespace client::ui
