#include "text_input.h"

#include "clay_ui_payloads.h"

#include <cstring>

namespace silencer::ui::primitives {

namespace {

constexpr int kPayloadCapacity = 64;
silencer::clay_bridge::TextInputPayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 64;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

// Per-frame slab for the password-mask buffers. Each TextInput emit may
// snapshot up to 256 chars (matches legacy TextInput::maxchars cap).
constexpr int kMaskBufferCount = 16;
constexpr int kMaskBufferLen   = 256;
char g_maskBuffers[kMaskBufferCount][kMaskBufferLen];
int g_maskBufferIndex = 0;

silencer::clay_bridge::TextInputPayload * AllocPayload() {
	if(g_payloadCount >= kPayloadCapacity) return nullptr;
	return &g_payloads[g_payloadCount++];
}

silencer::clay_bridge::ClayCustomData *
AllocCustomData(silencer::clay_bridge::CustomKind kind, void * payload) {
	if(g_customDataCount >= kCustomDataCapacity) return nullptr;
	auto * c = &g_customData[g_customDataCount++];
	c->kind = kind;
	c->payload = payload;
	return c;
}

char * AllocMaskBuffer() {
	if(g_maskBufferIndex >= kMaskBufferCount) return nullptr;
	return g_maskBuffers[g_maskBufferIndex++];
}

}  // namespace

void TextInputBeginFrame() {
	g_payloadCount = 0;
	g_customDataCount = 0;
	g_maskBufferIndex = 0;
}

void TextInput(Clay_String id,
               const char * text,
               TextInputOpts opts,
               TextInputHandle handle) {
	const char * src = text ? text : "";
	int srcLen = static_cast<int>(std::strlen(src));
	if(srcLen > kMaskBufferLen - 1) srcLen = kMaskBufferLen - 1;

	const char * displayText = src;
	if(opts.password){
		char * buf = AllocMaskBuffer();
		if(buf){
			for(int i = 0; i < srcLen; i++) buf[i] = '*';
			buf[srcLen] = '\0';
			displayText = buf;
		}
	}

	Uint8 brightness = opts.inactive ? static_cast<Uint8>(64) : opts.brightness;
	bool  showCaret  = !opts.inactive && opts.showCaret;
	Uint8 caretHeight =
		static_cast<Uint8>(static_cast<int>(opts.heightPx) * 4 / 5);  // h * 0.8

	auto * payload = AllocPayload();
	if(payload){
		payload->text        = displayText;
		payload->textLen     = static_cast<Uint16>(srcLen);
		payload->bank        = opts.fontBank;
		payload->fontWidth   = opts.fontWidth;
		payload->effectColor = opts.effectColor;
		payload->brightness  = brightness;
		payload->caretColor  = opts.caretColor;
		payload->caretHeight = caretHeight;
		payload->showCaret   = showCaret;
	}
	auto * ccd = AllocCustomData(
		silencer::clay_bridge::CustomKind::TextInput, payload);

	const float w = static_cast<float>(opts.widthPx);
	const float h = static_cast<float>(opts.heightPx);
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h) },
	       },
	       .custom = { .customData = ccd } }) {
		bool hovered = ::Clay_Hovered();
		if(handle.hoveredOut) *handle.hoveredOut = hovered;
	}
}

bool TextInputDispatchKey(const TextInputHandle & handle, char ch) {
	if(ch == '\n' || ch == '\r'){
		if(handle.onEnter) handle.onEnter(handle.user);
		return true;
	}
	return false;
}

}  // namespace silencer::ui::primitives
