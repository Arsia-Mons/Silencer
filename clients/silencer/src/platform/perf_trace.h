#pragma once
#include <SDL3/SDL.h>
#include <cstdint>

// Lightweight frame-time tracer. Enabled at runtime by setting the SILENCER_PERF
// env var (any value). When off, every hook below early-outs to a single bool
// test, so the scopes can stay compiled into release builds.
//
// Usage:
//   PERF_SCOPE("ui");                 // times the enclosing block
//   perf::FrameMark();                // call once per rendered frame
//
// Once per second the accumulated per-section time is printed to stdout, sorted
// by cost, as average ms-per-frame over the window, e.g.:
//   [perf] 60fps frame 16.62ms | present 14.90 | ui 1.31 | ui.raster 0.92 | net 0.02
namespace perf {

extern bool g_enabled;

void AddSample(const char * name, uint64_t ticks);
void FrameMark();

struct Scope {
	const char * name;
	uint64_t start;
	explicit Scope(const char * n) : name(n), start(g_enabled ? SDL_GetPerformanceCounter() : 0) {}
	~Scope(){ if(g_enabled) AddSample(name, SDL_GetPerformanceCounter() - start); }
};

} // namespace perf

#define PERF_CONCAT_(a, b) a##b
#define PERF_CONCAT(a, b) PERF_CONCAT_(a, b)
#define PERF_SCOPE(name) perf::Scope PERF_CONCAT(_perf_scope_, __LINE__)(name)
