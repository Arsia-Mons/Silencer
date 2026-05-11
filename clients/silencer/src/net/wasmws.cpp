#include "wasmws.h"

#ifdef __EMSCRIPTEN__

#include <cstring>
#include <emscripten/websocket.h>

WasmWS::WasmWS() = default;

WasmWS::~WasmWS() {
	Close();
}

bool WasmWS::Open(const char *url) {
	if (!emscripten_websocket_is_supported()) {
		state = STATE_FAILED;
		return false;
	}
	EmscriptenWebSocketCreateAttributes attrs;
	memset(&attrs, 0, sizeof(attrs));
	attrs.url = url;
	attrs.protocols = nullptr;          // any subprotocol the server picks is fine
	attrs.createOnMainThread = EM_TRUE; // we're single-threaded anyway

	sockId = emscripten_websocket_new(&attrs);
	if (sockId <= 0) {
		state = STATE_FAILED;
		sockId = -1;
		return false;
	}

	emscripten_websocket_set_onopen_callback(sockId, this, &WasmWS::OnOpen);
	emscripten_websocket_set_onmessage_callback(sockId, this, &WasmWS::OnMessage);
	emscripten_websocket_set_onclose_callback(sockId, this, &WasmWS::OnClose);
	emscripten_websocket_set_onerror_callback(sockId, this, &WasmWS::OnError);

	state = STATE_CONNECTING;
	return true;
}

void WasmWS::Close() {
	if (sockId > 0) {
		emscripten_websocket_close(sockId, 1000, "client closing");
		emscripten_websocket_delete(sockId);
		sockId = -1;
	}
	state = STATE_CLOSED;
}

bool WasmWS::PopBinary(std::vector<unsigned char> &out) {
	if (binaryQueue.empty()) return false;
	out = std::move(binaryQueue.front());
	binaryQueue.erase(binaryQueue.begin());
	return true;
}

bool WasmWS::PopText(std::string &out) {
	if (textQueue.empty()) return false;
	out = std::move(textQueue.front());
	textQueue.erase(textQueue.begin());
	return true;
}

bool WasmWS::SendText(const std::string &payload) {
	if (sockId <= 0 || state != STATE_OPEN) return false;
	EMSCRIPTEN_RESULT rc = emscripten_websocket_send_utf8_text(sockId, payload.c_str());
	return rc >= 0;
}

bool WasmWS::SendBinary(const unsigned char *bytes, size_t len) {
	if (sockId <= 0 || state != STATE_OPEN) return false;
	EMSCRIPTEN_RESULT rc = emscripten_websocket_send_binary(sockId, (void *)bytes, len);
	return rc >= 0;
}

bool WasmWS::OnOpen(int /*eventType*/, const EmscriptenWebSocketOpenEvent * /*e*/, void *userData) {
	WasmWS *self = static_cast<WasmWS *>(userData);
	self->state = STATE_OPEN;
	return true;
}

bool WasmWS::OnMessage(int /*eventType*/, const EmscriptenWebSocketMessageEvent *m, void *userData) {
	WasmWS *self = static_cast<WasmWS *>(userData);
	if (m->isText) {
		// data is a null-terminated UTF-8 string; numBytes includes the null.
		const char *s = reinterpret_cast<const char *>(m->data);
		size_t n = m->numBytes;
		if (n > 0 && s[n - 1] == '\0') n--;
		self->textQueue.emplace_back(s, n);
	} else {
		const unsigned char *bytes = static_cast<const unsigned char *>(m->data);
		self->binaryQueue.emplace_back(bytes, bytes + m->numBytes);
	}
	return true;
}

bool WasmWS::OnClose(int /*eventType*/, const EmscriptenWebSocketCloseEvent * /*e*/, void *userData) {
	WasmWS *self = static_cast<WasmWS *>(userData);
	self->state = STATE_CLOSED;
	return true;
}

bool WasmWS::OnError(int /*eventType*/, const EmscriptenWebSocketErrorEvent * /*e*/, void *userData) {
	WasmWS *self = static_cast<WasmWS *>(userData);
	self->state = STATE_FAILED;
	return true;
}

#endif // __EMSCRIPTEN__
