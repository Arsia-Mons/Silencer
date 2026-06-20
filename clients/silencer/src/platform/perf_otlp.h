#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// OTLP/HTTP exporter for the perf tracer (Layer 1 metrics + Layer 2 traces).
//
// Off unless SILENCER_PERF_OTLP=<base-url> is set, e.g.
//   SILENCER_PERF_OTLP=http://localhost:5080/api/default
// Auth (OpenObserve root basic-auth or an ingestion token):
//   SILENCER_PERF_OTLP_USER + SILENCER_PERF_OTLP_PASS  → Basic <base64(user:pass)>
//   SILENCER_PERF_OTLP_AUTH=<verbatim Authorization header value>  (overrides)
//
// Identity/device live in the OTLP Resource block (set once per session, sent
// once per export request). Only per-operation-varying data (span name, "map",
// frame duration) rides on span attributes. The frame thread enqueues completed
// traces / metric snapshots into a bounded queue; one background thread batches
// them into curl POSTs. Telemetry never blocks a frame — overflow drops.

namespace perf {
namespace otlp {

struct Attr {
	const char * key;
	enum Type { Str, Int, Dbl, Bool } type;
	std::string s;
	long long i = 0;
	double d = 0.0;
	bool b = false;
};
inline Attr SA(const char * k, std::string v) { Attr a; a.key = k; a.type = Attr::Str; a.s = std::move(v); return a; }
inline Attr IA(const char * k, long long v)   { Attr a; a.key = k; a.type = Attr::Int; a.i = v; return a; }
inline Attr DA(const char * k, double v)      { Attr a; a.key = k; a.type = Attr::Dbl; a.d = v; return a; }
inline Attr BA(const char * k, bool v)        { Attr a; a.key = k; a.type = Attr::Bool; a.b = v; return a; }

struct Span {
	uint64_t id;
	uint64_t parent; // 0 == trace root
	const char * name;
	uint64_t start_ns;
	uint64_t end_ns;
	std::vector<Attr> attrs; // usually empty; only operation roots carry a few
};

struct Trace {
	uint64_t trace_hi;
	uint64_t trace_lo;
	std::vector<Span> spans;
};

struct Metrics {
	uint64_t ts_ns;
	std::vector<std::pair<const char *, double>> gauges;
};

// Constant-per-session identity + device data → OTLP Resource attributes.
struct ResourceInfo {
	std::string service_name;
	std::string build_version;
	std::string account_id; // empty when not yet known
	std::string os;
	std::string gpu;
	std::string gpu_driver;
	std::string cpu;
	int cpu_cores = 0;
	int ram_mb = 0;
	int disp_w = 0, disp_h = 0, refresh = 0;
	int viewport_w = 0, viewport_h = 0;
	bool fullscreen = false;
	bool vsync = true;
	std::string ui_path; // "gpu" | "cpu"
};

// Start the exporter. `endpoint` is the SILENCER_PERF_OTLP base URL. Returns
// true if a sender thread is running and traces/metrics will be exported.
bool Init(const char * endpoint);
void Shutdown();
bool Active();

// A fresh UUID minted per launch; surfaced so a slow player can report it.
const std::string & SessionId();

// Resource lifecycle. SetResource installs the full session-constant block
// (session.id + session.epoch are added internally). The narrow setters below
// handle the few fields that can change mid-session — each bumps session.epoch
// so every epoch's Resource is internally constant.
void SetResource(const ResourceInfo & r);
void SetAccountId(uint64_t account_id);
void SetDisplay(int viewport_w, int viewport_h, bool fullscreen, bool vsync,
                const char * ui_path);

void EnqueueTrace(Trace && t);
void EnqueueMetrics(Metrics && m);

} // namespace otlp
} // namespace perf
