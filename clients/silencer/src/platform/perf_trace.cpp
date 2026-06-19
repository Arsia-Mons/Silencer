#include "perf_trace.h"
#include <cstdio>

namespace perf {

bool g_enabled = false;

namespace {

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

Acc * Find(const char * name){
	for(int i = 0; i < g_count; ++i)
		if(g_acc[i].name == name) return &g_acc[i];
	if(g_count >= kMaxSections) return nullptr;
	g_acc[g_count] = {name, 0, 0};
	return &g_acc[g_count++];
}

void EnsureInit(){
	if(g_inited) return;
	g_inited = true;
	g_enabled = SDL_getenv("SILENCER_PERF") != nullptr;
	g_msPerTick = 1000.0 / (double)SDL_GetPerformanceFrequency();
	g_windowStart = SDL_GetPerformanceCounter();
}

} // namespace

void AddSample(const char * name, uint64_t ticks){
	if(!g_enabled) return;
	Acc * a = Find(name);
	if(a){ a->ticks += ticks; a->calls += 1; }
}

void FrameMark(){
	EnsureInit();
	if(!g_enabled) return;
	g_frames += 1;
	uint64_t now = SDL_GetPerformanceCounter();
	double elapsedMs = (double)(now - g_windowStart) * g_msPerTick;
	if(elapsedMs < 1000.0) return;

	// Sort sections by cost (descending) so the priciest reads first.
	int idx[kMaxSections];
	for(int i = 0; i < g_count; ++i) idx[i] = i;
	for(int i = 0; i < g_count; ++i)
		for(int j = i + 1; j < g_count; ++j)
			if(g_acc[idx[j]].ticks > g_acc[idx[i]].ticks){ int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }

	char line[640];
	int o = snprintf(line, sizeof(line), "[perf] %ufps frame %.2fms",
	                 (unsigned)g_frames, elapsedMs / (double)g_frames);
	for(int k = 0; k < g_count && o < (int)sizeof(line); ++k){
		Acc & a = g_acc[idx[k]];
		double avgMs = ((double)a.ticks * g_msPerTick) / (double)g_frames;
		o += snprintf(line + o, sizeof(line) - o, " | %s %.2f", a.name, avgMs);
	}
	printf("%s\n", line);
	fflush(stdout);

	for(int i = 0; i < g_count; ++i){ g_acc[i].ticks = 0; g_acc[i].calls = 0; }
	g_frames = 0;
	g_windowStart = now;
}

} // namespace perf
