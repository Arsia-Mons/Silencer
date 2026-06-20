#include "perf_trace.h"
#include "perf_otlp.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace perf {

bool g_enabled = false;
bool g_trace_enabled = false;

namespace {

// ---- stdout aggregate (Layer-0, unchanged) -------------------------------
struct Acc {
	const char * name; // string-literal pointer identity is the key (no strcmp)
	uint64_t ticks;
	uint32_t calls;
};

constexpr int kMaxSections = 32;
Acc g_acc[kMaxSections];
int g_count = 0;
uint32_t g_frames = 0;
uint64_t g_windowStart = 0;
double g_msPerTick = 0.0;
bool g_inited = false;

// ---- wall-clock anchor (perf counter is monotonic, not wall time) --------
uint64_t g_anchorWallNs = 0;
uint64_t g_anchorCounter = 0;
double g_nsPerTick = 0.0;

// ---- metrics window (Layer-1) --------------------------------------------
uint64_t g_lastMarkCounter = 0;
std::vector<float> g_frameMs; // per-frame periods since last metrics flush
constexpr double kWindowMs = 1000.0;

// ---- frame-trace sampling (Layer-2) --------------------------------------
double g_frameBudgetMs = 16.67; // ~60 fps target
double g_frameOverFactor = 2.0; // emit a frame trace when frame_ms > factor*budget
uint32_t g_frameBaselineN = 600; // also emit 1-in-N frames as a baseline (0 = off)
uint64_t g_frameCounter = 0;

// ---- trace builders (a stack of in-flight operations) --------------------
struct PendingSpan {
	uint64_t id;
	uint64_t parent;
	const char * name;
	uint64_t start_ns;
	uint64_t end_ns;
	std::vector<otlp::Attr> attrs;
};
struct Builder {
	uint64_t trace_hi, trace_lo;
	Sampling mode;
	std::vector<PendingSpan> spans;
	std::vector<int> open; // indices of currently-open spans (parent chain)
};
std::vector<Builder> g_ops; // active operations; back() is the current trace

// ---- splitmix64 id generator (per-session seed, main thread only) --------
uint64_t g_idState = 0;
uint64_t NextId() {
	uint64_t z = (g_idState += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31);
}

Acc * Find(const char * name) {
	for (int i = 0; i < g_count; ++i)
		if (g_acc[i].name == name) return &g_acc[i];
	if (g_count >= kMaxSections) return nullptr;
	g_acc[g_count] = {name, 0, 0};
	return &g_acc[g_count++];
}

double EnvDouble(const char * key, double fallback) {
	const char * v = SDL_getenv(key);
	if (!v || !*v) return fallback;
	double d = SDL_atof(v);
	return d > 0.0 ? d : fallback;
}

double Percentile(std::vector<float> & sorted, double pct) {
	if (sorted.empty()) return 0.0;
	size_t idx = (size_t)(pct / 100.0 * (double)(sorted.size() - 1) + 0.5);
	if (idx >= sorted.size()) idx = sorted.size() - 1;
	return sorted[idx];
}

void FlushMetrics(uint64_t now) {
	if (!g_trace_enabled || g_frames == 0) return;
	double elapsedMs = (double)(now - g_windowStart) * g_msPerTick;
	double elapsedS = elapsedMs / 1000.0;

	otlp::Metrics m;
	m.ts_ns = CounterToWallNs(now);
	m.gauges.push_back({"frame.fps", elapsedS > 0 ? (double)g_frames / elapsedS : 0.0});

	std::vector<float> sorted = g_frameMs;
	std::sort(sorted.begin(), sorted.end());
	m.gauges.push_back({"frame.ms.p50", Percentile(sorted, 50.0)});
	m.gauges.push_back({"frame.ms.p95", Percentile(sorted, 95.0)});
	m.gauges.push_back({"frame.ms.p99", Percentile(sorted, 99.0)});
	m.gauges.push_back({"frame.ms.worst", sorted.empty() ? 0.0 : sorted.back()});

	// Per-section avg ms/frame (present/ui/sim/world.draw/...): the same arena
	// the stdout layer prints, exported as low-cardinality gauges.
	for (int i = 0; i < g_count; ++i) {
		double avgMs = ((double)g_acc[i].ticks * g_msPerTick) / (double)g_frames;
		m.gauges.push_back({g_acc[i].name, avgMs});
	}
	otlp::EnqueueMetrics(std::move(m));
}

} // namespace

uint64_t CounterToWallNs(uint64_t counter) {
	if (counter < g_anchorCounter) return g_anchorWallNs;
	return g_anchorWallNs + (uint64_t)((double)(counter - g_anchorCounter) * g_nsPerTick);
}

void Init() {
	if (g_inited) return;
	g_inited = true;
	g_enabled = SDL_getenv("SILENCER_PERF") != nullptr;
	const char * otlp_ep = SDL_getenv("SILENCER_PERF_OTLP");

	g_msPerTick = 1000.0 / (double)SDL_GetPerformanceFrequency();
	g_nsPerTick = 1.0e9 / (double)SDL_GetPerformanceFrequency();
	uint64_t counter = SDL_GetPerformanceCounter();
	g_anchorCounter = counter;
	g_windowStart = counter;
	g_lastMarkCounter = counter;
	g_anchorWallNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
	                     std::chrono::system_clock::now().time_since_epoch())
	                     .count();
	g_idState = g_anchorWallNs ^ (counter * 0x9e3779b97f4a7c15ull);

	if (otlp_ep && *otlp_ep) {
		g_frameBudgetMs = EnvDouble("SILENCER_PERF_FRAME_BUDGET_MS", g_frameBudgetMs);
		g_frameOverFactor = EnvDouble("SILENCER_PERF_FRAME_OVER_FACTOR", g_frameOverFactor);
		const char * n = SDL_getenv("SILENCER_PERF_FRAME_BASELINE_N");
		if (n && *n) g_frameBaselineN = (uint32_t)SDL_atoi(n);
		g_trace_enabled = otlp::Init(otlp_ep);
		if (g_trace_enabled) {
			printf("[perf] OTLP export -> %s | session.id=%s\n", otlp_ep,
			       otlp::SessionId().c_str());
			fflush(stdout);
		}
	}
}

void Shutdown() {
	if (g_trace_enabled) otlp::Shutdown();
	g_trace_enabled = false;
}

void AddSample(const char * name, uint64_t ticks) {
	if (!g_enabled && !g_trace_enabled) return;
	Acc * a = Find(name);
	if (a) { a->ticks += ticks; a->calls += 1; }
}

bool ScopeBegin(const char * name, uint64_t start_counter) {
	if (g_ops.empty()) return false; // no active operation → not traced
	Builder & b = g_ops.back();
	uint64_t parent = b.spans[b.open.back()].id;
	int idx = (int)b.spans.size();
	b.spans.push_back({NextId(), parent, name, CounterToWallNs(start_counter), 0, {}});
	b.open.push_back(idx);
	return true;
}

void ScopeEnd(uint64_t end_counter) {
	if (g_ops.empty()) return;
	Builder & b = g_ops.back();
	if (b.open.size() <= 1) return; // never pop the root via a scope
	int idx = b.open.back();
	b.open.pop_back();
	b.spans[idx].end_ns = CounterToWallNs(end_counter);
}

void OperationBegin(const char * name, Sampling mode, const char * attr_key,
                    const char * attr_val) {
	Builder b;
	b.trace_hi = NextId();
	b.trace_lo = NextId();
	b.mode = mode;
	PendingSpan root;
	root.id = NextId();
	root.parent = 0;
	root.name = name;
	root.start_ns = CounterToWallNs(SDL_GetPerformanceCounter());
	root.end_ns = 0;
	if (attr_key && attr_val) root.attrs.push_back(otlp::SA(attr_key, attr_val));
	b.spans.push_back(std::move(root));
	b.open.push_back(0);
	g_ops.push_back(std::move(b));
}

void OperationEnd() {
	if (g_ops.empty()) return;
	Builder b = std::move(g_ops.back());
	g_ops.pop_back();
	uint64_t end = CounterToWallNs(SDL_GetPerformanceCounter());
	b.spans[0].end_ns = end;
	// Close any scopes the caller left dangling (defensive).
	for (size_t i = 1; i < b.spans.size(); ++i)
		if (b.spans[i].end_ns == 0) b.spans[i].end_ns = end;

	bool emit = true;
	if (b.mode == Sampling::FrameBudget) {
		double durMs = (double)(b.spans[0].end_ns - b.spans[0].start_ns) / 1.0e6;
		bool overBudget = durMs > g_frameBudgetMs * g_frameOverFactor;
		bool baseline = g_frameBaselineN > 0 && (g_frameCounter % g_frameBaselineN == 0);
		++g_frameCounter;
		emit = overBudget || baseline;
	}
	if (!emit) return;

	otlp::Trace t;
	t.trace_hi = b.trace_hi;
	t.trace_lo = b.trace_lo;
	t.spans.reserve(b.spans.size());
	for (PendingSpan & ps : b.spans)
		t.spans.push_back({ps.id, ps.parent, ps.name, ps.start_ns, ps.end_ns, std::move(ps.attrs)});
	otlp::EnqueueTrace(std::move(t));
}

void FrameMark() {
	Init();
	if (!g_enabled && !g_trace_enabled) return;
	g_frames += 1;
	uint64_t now = SDL_GetPerformanceCounter();

	if (g_trace_enabled) {
		double ms = (double)(now - g_lastMarkCounter) * g_msPerTick;
		if (g_frameMs.size() < 4096) g_frameMs.push_back((float)ms);
	}
	g_lastMarkCounter = now;

	double elapsedMs = (double)(now - g_windowStart) * g_msPerTick;
	if (elapsedMs < kWindowMs) return;

	if (g_enabled) {
		// Sort sections by cost (descending) so the priciest reads first.
		int idx[kMaxSections];
		for (int i = 0; i < g_count; ++i) idx[i] = i;
		for (int i = 0; i < g_count; ++i)
			for (int j = i + 1; j < g_count; ++j)
				if (g_acc[idx[j]].ticks > g_acc[idx[i]].ticks) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }

		char line[640];
		int o = snprintf(line, sizeof(line), "[perf] %ufps frame %.2fms",
		                 (unsigned)g_frames, elapsedMs / (double)g_frames);
		for (int k = 0; k < g_count && o < (int)sizeof(line); ++k) {
			Acc & a = g_acc[idx[k]];
			double avgMs = ((double)a.ticks * g_msPerTick) / (double)g_frames;
			o += snprintf(line + o, sizeof(line) - o, " | %s %.2f", a.name, avgMs);
		}
		printf("%s\n", line);
		fflush(stdout);
	}

	FlushMetrics(now);

	for (int i = 0; i < g_count; ++i) { g_acc[i].ticks = 0; g_acc[i].calls = 0; }
	g_frames = 0;
	g_windowStart = now;
	g_frameMs.clear();
}

} // namespace perf
