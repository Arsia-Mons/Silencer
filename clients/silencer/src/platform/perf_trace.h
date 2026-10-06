#pragma once
#include <SDL3/SDL.h>
#include <cstdint>

// Two-layer perf tracer.
//
//  PERF_SCOPE("name")   — RAII timer for the enclosing block.
//  perf::Operation op   — RAII trace root; PERF_SCOPEs running inside nest
//                         under it via a thread-local scope stack.
//  perf::FrameMark()    — call once per rendered frame.
//
// Two independent outputs, each gated by its own env var (set in perf::Init):
//
//  SILENCER_PERF        → g_enabled: per-section avg ms printed to stdout once
//                         per window (unchanged legacy behavior).
//  SILENCER_PERF_OTLP   → g_trace_enabled: Layer-1 metrics + Layer-2 operation
//                         traces exported to OpenObserve over OTLP/HTTP.
//
// When both are unset every hook below early-outs on a single bool test, so the
// scopes stay compiled in at no cost.
namespace perf {

extern bool g_enabled;       // stdout aggregate
extern bool g_trace_enabled; // OTLP metrics + traces

// Anchor the monotonic clock to wall time, read the env switches, and start the
// OTLP exporter if configured. Idempotent; safe to call before any scope.
void Init();
void Shutdown();

// Absolute Unix wall-clock nanoseconds for a SDL_GetPerformanceCounter() value,
// derived from the init-time anchor (perf counter is monotonic, not wall-clock).
uint64_t CounterToWallNs(uint64_t counter);

void AddSample(const char * name, uint64_t ticks); // stdout aggregate
void FrameMark();                                   // per-frame stdout + metrics

// --- tracing internals (used by Scope / Operation below) ---
enum class Sampling { Always, FrameBudget };
bool ScopeBegin(const char * name, uint64_t start_counter); // true if a span was opened
void ScopeEnd(uint64_t end_counter);
void OperationBegin(const char * name, Sampling mode, const char * attr_key,
                    const char * attr_val);
void OperationEnd();

// Opens a trace ROOT for one logical operation (frame / startup / level_load).
// Any PERF_SCOPE running inside nests under it. FrameBudget operations are
// tail-sampled (emitted only when over budget, plus a 1-in-N baseline); Always
// operations always emit.
struct Operation {
	bool active;
	Operation(const char * name, Sampling mode)
	    : active(g_trace_enabled) {
		if (active) OperationBegin(name, mode, nullptr, nullptr);
	}
	Operation(const char * name, Sampling mode, const char * attr_key, const char * attr_val)
	    : active(g_trace_enabled) {
		if (active) OperationBegin(name, mode, attr_key, attr_val);
	}
	// `enabled` lets a call site keep the RAII span scoping while opting out
	// (e.g. the dedicated server never opens a frame trace).
	Operation(const char * name, Sampling mode, bool enabled)
	    : active(g_trace_enabled && enabled) {
		if (active) OperationBegin(name, mode, nullptr, nullptr);
	}
	~Operation() { if (active) OperationEnd(); }
	Operation(const Operation &) = delete;
	Operation & operator=(const Operation &) = delete;
};

struct Scope {
	const char * name;
	uint64_t start;
	bool traced;
	explicit Scope(const char * n)
	    : name(n),
	      start((g_enabled || g_trace_enabled) ? SDL_GetPerformanceCounter() : 0),
	      traced(false) {
		if (g_trace_enabled) traced = ScopeBegin(n, start);
	}
	~Scope() {
		if (!g_enabled && !g_trace_enabled) return;
		uint64_t end = SDL_GetPerformanceCounter();
		// Feed the per-section aggregate (drives both the stdout line and the
		// Layer-1 metric gauges); close the span only if one was opened.
		AddSample(name, end - start);
		if (traced) ScopeEnd(end);
	}
};

} // namespace perf

#define PERF_CONCAT_(a, b) a##b
#define PERF_CONCAT(a, b) PERF_CONCAT_(a, b)
#define PERF_SCOPE(name) perf::Scope PERF_CONCAT(_perf_scope_, __LINE__)(name)
