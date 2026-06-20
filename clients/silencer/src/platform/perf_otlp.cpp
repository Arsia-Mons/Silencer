#include "perf_otlp.h"

#include <SDL3/SDL.h>
#include <curl/curl.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace perf {
namespace otlp {

namespace {

// ---- config + lifecycle --------------------------------------------------
std::string g_tracesUrl;
std::string g_metricsUrl;
std::string g_authHeader; // full "Authorization: ..." line, or empty
std::string g_sessionId;
std::atomic<bool> g_running{false};

// ---- resource (constant per session.epoch) -------------------------------
std::mutex g_resMutex;
ResourceInfo g_res;
uint64_t g_epoch = 0;
std::shared_ptr<const std::string> g_resJson; // cached "{\"attributes\":[...]}"

// ---- export queue --------------------------------------------------------
struct Item {
	bool is_metric;
	Trace trace;
	Metrics metric;
	std::shared_ptr<const std::string> res;
};
constexpr size_t kMaxQueue = 4096;
std::mutex g_qMutex;
std::condition_variable g_qCv;
std::deque<Item> g_queue;
bool g_stop = false;
uint64_t g_dropped = 0;
std::thread g_thread;

// ---- string helpers ------------------------------------------------------
void AppendEscaped(std::string & out, const std::string & s) {
	for (char c : s) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if ((unsigned char)c < 0x20) {
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
				out += buf;
			} else {
				out += c;
			}
		}
	}
}

void AppendHex64(std::string & out, uint64_t v) {
	char buf[17];
	snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)v);
	out += buf;
}

void AppendU64(std::string & out, uint64_t v) {
	char buf[24];
	snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
	out += buf;
}

void AppendDouble(std::string & out, double v) {
	char buf[40];
	snprintf(buf, sizeof(buf), "%.6g", v);
	out += buf;
}

std::string Base64(const std::string & in) {
	static const char * tbl =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	int val = 0, bits = -6;
	for (unsigned char c : in) {
		val = (val << 8) + c;
		bits += 8;
		while (bits >= 0) {
			out += tbl[(val >> bits) & 0x3F];
			bits -= 6;
		}
	}
	if (bits > -6) out += tbl[((val << 8) >> (bits + 8)) & 0x3F];
	while (out.size() % 4) out += '=';
	return out;
}

void AppendAttr(std::string & out, const Attr & a) {
	out += "{\"key\":\"";
	out += a.key;
	out += "\",\"value\":{";
	switch (a.type) {
	case Attr::Str:
		out += "\"stringValue\":\"";
		AppendEscaped(out, a.s);
		out += "\"";
		break;
	case Attr::Int:
		out += "\"intValue\":\"";
		{ char b[24]; snprintf(b, sizeof(b), "%lld", a.i); out += b; }
		out += "\"";
		break;
	case Attr::Dbl:
		out += "\"doubleValue\":";
		AppendDouble(out, a.d);
		break;
	case Attr::Bool:
		out += a.b ? "\"boolValue\":true" : "\"boolValue\":false";
		break;
	}
	out += "}}";
}

// Build the resource "{\"attributes\":[...]}" block from the current info.
std::shared_ptr<const std::string> BuildResJsonLocked() {
	std::vector<Attr> attrs;
	attrs.push_back(SA("service.name", g_res.service_name.empty() ? "silencer" : g_res.service_name));
	attrs.push_back(SA("session.id", g_sessionId));
	attrs.push_back(IA("session.epoch", (long long)g_epoch));
	if (!g_res.build_version.empty()) attrs.push_back(SA("build.version", g_res.build_version));
	if (!g_res.account_id.empty()) attrs.push_back(SA("account.id", g_res.account_id));
	if (!g_res.os.empty()) attrs.push_back(SA("device.os", g_res.os));
	if (!g_res.gpu.empty()) attrs.push_back(SA("device.gpu", g_res.gpu));
	if (!g_res.gpu_driver.empty()) attrs.push_back(SA("device.gpu_driver", g_res.gpu_driver));
	if (!g_res.cpu.empty()) attrs.push_back(SA("device.cpu", g_res.cpu));
	if (g_res.cpu_cores > 0) attrs.push_back(IA("device.cpu_cores", g_res.cpu_cores));
	if (g_res.ram_mb > 0) attrs.push_back(IA("device.ram_mb", g_res.ram_mb));
	if (g_res.disp_w > 0) {
		char b[32]; snprintf(b, sizeof(b), "%dx%d", g_res.disp_w, g_res.disp_h);
		attrs.push_back(SA("display.resolution", b));
	}
	if (g_res.viewport_w > 0) {
		char b[32]; snprintf(b, sizeof(b), "%dx%d", g_res.viewport_w, g_res.viewport_h);
		attrs.push_back(SA("display.viewport", b));
	}
	if (g_res.refresh > 0) attrs.push_back(IA("display.refresh", g_res.refresh));
	attrs.push_back(BA("render.vsync", g_res.vsync));
	attrs.push_back(BA("render.fullscreen", g_res.fullscreen));
	if (!g_res.ui_path.empty()) attrs.push_back(SA("render.ui_path", g_res.ui_path));

	auto s = std::make_shared<std::string>();
	*s += "{\"attributes\":[";
	for (size_t i = 0; i < attrs.size(); ++i) {
		if (i) *s += ",";
		AppendAttr(*s, attrs[i]);
	}
	*s += "]}";
	return s;
}

std::shared_ptr<const std::string> CurrentResJson() {
	std::lock_guard<std::mutex> lk(g_resMutex);
	if (!g_resJson) g_resJson = BuildResJsonLocked();
	return g_resJson;
}

// ---- sender thread -------------------------------------------------------
void AppendSpan(std::string & out, const Trace & t, const Span & s) {
	out += "{\"traceId\":\"";
	AppendHex64(out, t.trace_hi);
	AppendHex64(out, t.trace_lo);
	out += "\",\"spanId\":\"";
	AppendHex64(out, s.id);
	out += "\",";
	if (s.parent != 0) {
		out += "\"parentSpanId\":\"";
		AppendHex64(out, s.parent);
		out += "\",";
	}
	out += "\"name\":\"";
	AppendEscaped(out, std::string(s.name ? s.name : ""));
	out += "\",\"kind\":1,\"startTimeUnixNano\":\"";
	AppendU64(out, s.start_ns);
	out += "\",\"endTimeUnixNano\":\"";
	AppendU64(out, s.end_ns);
	out += "\"";
	if (!s.attrs.empty()) {
		out += ",\"attributes\":[";
		for (size_t i = 0; i < s.attrs.size(); ++i) {
			if (i) out += ",";
			AppendAttr(out, s.attrs[i]);
		}
		out += "]";
	}
	out += "}";
}

size_t DiscardBody(char *, size_t sz, size_t nmemb, void *) { return sz * nmemb; }

void Post(CURL * curl, struct curl_slist * headers, const std::string & url,
          const std::string & body) {
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DiscardBody);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	CURLcode rc = curl_easy_perform(curl);
	if (rc != CURLE_OK) {
		fprintf(stderr, "[perf-otlp] POST %s failed: %s\n", url.c_str(),
		        curl_easy_strerror(rc));
	}
}

// Flush accumulated trace spans (all sharing one resource) as one request.
void FlushTraceBatch(CURL * curl, struct curl_slist * headers,
                     const std::shared_ptr<const std::string> & res,
                     std::string & spans, int & count) {
	if (count == 0) return;
	std::string body = "{\"resourceSpans\":[{\"resource\":";
	body += *res;
	body += ",\"scopeSpans\":[{\"scope\":{\"name\":\"silencer.perf\"},\"spans\":[";
	body += spans;
	body += "]}]}]}";
	Post(curl, headers, g_tracesUrl, body);
	spans.clear();
	count = 0;
}

void PostMetrics(CURL * curl, struct curl_slist * headers,
                 const std::shared_ptr<const std::string> & res, const Metrics & m) {
	std::string body = "{\"resourceMetrics\":[{\"resource\":";
	body += *res;
	body += ",\"scopeMetrics\":[{\"scope\":{\"name\":\"silencer.perf\"},\"metrics\":[";
	for (size_t i = 0; i < m.gauges.size(); ++i) {
		if (i) body += ",";
		body += "{\"name\":\"";
		AppendEscaped(body, std::string(m.gauges[i].first));
		body += "\",\"gauge\":{\"dataPoints\":[{\"asDouble\":";
		AppendDouble(body, m.gauges[i].second);
		body += ",\"timeUnixNano\":\"";
		AppendU64(body, m.ts_ns);
		body += "\"}]}}";
	}
	body += "]}]}]}";
	Post(curl, headers, g_metricsUrl, body);
}

void SenderLoop() {
	CURL * curl = curl_easy_init();
	struct curl_slist * headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (!g_authHeader.empty()) headers = curl_slist_append(headers, g_authHeader.c_str());

	constexpr int kBatchSpanCap = 256;
	while (true) {
		std::deque<Item> batch;
		{
			std::unique_lock<std::mutex> lk(g_qMutex);
			g_qCv.wait_for(lk, std::chrono::milliseconds(500),
			               [] { return g_stop || !g_queue.empty(); });
			batch.swap(g_queue);
			bool stop = g_stop;
			lk.unlock();
			if (batch.empty() && stop) break;
		}

		// Accumulate trace spans sharing a resource into batched POSTs.
		std::shared_ptr<const std::string> curRes;
		std::string spanBuf;
		int spanCount = 0;
		for (Item & it : batch) {
			if (it.is_metric) {
				if (curl) PostMetrics(curl, headers, it.res, it.metric);
				continue;
			}
			if (curRes && it.res != curRes && spanCount > 0)
				FlushTraceBatch(curl, headers, curRes, spanBuf, spanCount);
			curRes = it.res;
			for (const Span & s : it.trace.spans) {
				if (spanCount > 0) spanBuf += ",";
				AppendSpan(spanBuf, it.trace, s);
				if (++spanCount >= kBatchSpanCap)
					FlushTraceBatch(curl, headers, curRes, spanBuf, spanCount);
			}
		}
		if (curl && spanCount > 0) FlushTraceBatch(curl, headers, curRes, spanBuf, spanCount);

		{
			std::lock_guard<std::mutex> lk(g_qMutex);
			if (g_stop && g_queue.empty()) break;
		}
	}

	if (headers) curl_slist_free_all(headers);
	if (curl) curl_easy_cleanup(curl);
}

std::string MakeUuid() {
	// 122-bit random v4 UUID from two SDL_GetPerformanceCounter()-seeded draws.
	uint64_t s = (uint64_t)SDL_GetPerformanceCounter() ^ 0x9e3779b97f4a7c15ull;
	auto next = [&]() {
		uint64_t z = (s += 0x9e3779b97f4a7c15ull);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
		return z ^ (z >> 31);
	};
	uint64_t hi = next(), lo = next();
	hi = (hi & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull; // version 4
	lo = (lo & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull; // variant
	char buf[40];
	snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
	         (unsigned)(hi >> 32), (unsigned)((hi >> 16) & 0xFFFF),
	         (unsigned)(hi & 0xFFFF), (unsigned)((lo >> 48) & 0xFFFF),
	         (unsigned long long)(lo & 0xFFFFFFFFFFFFull));
	return buf;
}

} // namespace

bool Init(const char * endpoint) {
	if (g_running.load()) return true;
	if (!endpoint || !*endpoint) return false;

	std::string base = endpoint;
	while (!base.empty() && base.back() == '/') base.pop_back();
	g_tracesUrl = base + "/v1/traces";
	g_metricsUrl = base + "/v1/metrics";

	const char * rawAuth = SDL_getenv("SILENCER_PERF_OTLP_AUTH");
	if (rawAuth && *rawAuth) {
		g_authHeader = std::string("Authorization: ") + rawAuth;
	} else {
		const char * user = SDL_getenv("SILENCER_PERF_OTLP_USER");
		const char * pass = SDL_getenv("SILENCER_PERF_OTLP_PASS");
		if (user && pass) {
			g_authHeader = "Authorization: Basic " +
			               Base64(std::string(user) + ":" + pass);
		}
	}

	// curl global init on the main thread before the sender thread spins up
	// (curl_easy_init's lazy global init is not thread-safe).
	curl_global_init(CURL_GLOBAL_DEFAULT);

	g_sessionId = MakeUuid();
	g_stop = false;
	g_running.store(true);
	g_thread = std::thread(SenderLoop);
	return true;
}

void Shutdown() {
	if (!g_running.load()) return;
	{
		std::lock_guard<std::mutex> lk(g_qMutex);
		g_stop = true;
	}
	g_qCv.notify_all();
	if (g_thread.joinable()) g_thread.join();
	g_running.store(false);
	if (g_dropped) fprintf(stderr, "[perf-otlp] dropped %llu telemetry items (queue overflow)\n",
	                       (unsigned long long)g_dropped);
}

bool Active() { return g_running.load(); }

const std::string & SessionId() { return g_sessionId; }

void SetResource(const ResourceInfo & r) {
	std::lock_guard<std::mutex> lk(g_resMutex);
	g_res = r;
	g_resJson = BuildResJsonLocked();
}

void SetAccountId(uint64_t account_id) {
	std::lock_guard<std::mutex> lk(g_resMutex);
	std::string s = std::to_string(account_id);
	if (g_res.account_id == s) return;
	g_res.account_id = s;
	++g_epoch;
	g_resJson = BuildResJsonLocked();
}

void SetDisplay(int viewport_w, int viewport_h, bool fullscreen, bool vsync,
                const char * ui_path) {
	std::lock_guard<std::mutex> lk(g_resMutex);
	std::string up = ui_path ? ui_path : "";
	if (g_res.viewport_w == viewport_w && g_res.viewport_h == viewport_h &&
	    g_res.fullscreen == fullscreen && g_res.vsync == vsync && g_res.ui_path == up)
		return;
	g_res.viewport_w = viewport_w;
	g_res.viewport_h = viewport_h;
	g_res.fullscreen = fullscreen;
	g_res.vsync = vsync;
	g_res.ui_path = up;
	++g_epoch;
	g_resJson = BuildResJsonLocked();
}

void EnqueueTrace(Trace && t) {
	if (!g_running.load()) return;
	auto res = CurrentResJson();
	std::lock_guard<std::mutex> lk(g_qMutex);
	if (g_queue.size() >= kMaxQueue) { ++g_dropped; return; }
	Item it;
	it.is_metric = false;
	it.trace = std::move(t);
	it.res = res;
	g_queue.push_back(std::move(it));
	g_qCv.notify_one();
}

void EnqueueMetrics(Metrics && m) {
	if (!g_running.load()) return;
	auto res = CurrentResJson();
	std::lock_guard<std::mutex> lk(g_qMutex);
	if (g_queue.size() >= kMaxQueue) { ++g_dropped; return; }
	Item it;
	it.is_metric = true;
	it.metric = std::move(m);
	it.res = res;
	g_queue.push_back(std::move(it));
	g_qCv.notify_one();
}

} // namespace otlp
} // namespace perf
