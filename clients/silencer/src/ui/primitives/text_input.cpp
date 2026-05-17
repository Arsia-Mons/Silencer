#include "text_input.h"

#include "clay_ui_payloads.h"
#include "primitives/text_internal.h"
#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <cstring>
#include <string>

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

std::string ToStd(Clay_String text) {
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(text.length));
}

void RegisterTextInputWidget(Clay_String clayId,
                             const char * value,
                             const TextInputOpts& opts,
                             const TextInputHandle& handle) {
	if(!handle.interactions) return;
	silencer::ui::UiInteractable widget;
	widget.id = (handle.actionId && *handle.actionId) ? handle.actionId : ToStd(clayId);
	widget.labelText = handle.label ? handle.label : "";
	widget.kind = silencer::ui::UiInteractableKind::TextInput;
	widget.uid = handle.uid;
	widget.clayId = CLAY_SID(clayId);
	widget.hasClayId = true;
	widget.value = value ? value : "";
	widget.maxLength = handle.maxLength;
	widget.isPassword = opts.password;
	widget.inactive = opts.inactive;
	widget.numbersOnly = opts.numbersOnly;
	widget.cancelOnEscape = handle.cancelOnEscape;
	handle.interactions->RegisterInteractable(widget);
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

	const auto style = text_internal::ResolveTextRenderStyle(opts.textSize);
	const int visibleWidthPx = std::max(
		0,
		static_cast<int>(opts.widthPx) - static_cast<int>(opts.contentInsetX));
	int visibleChars = visibleWidthPx > 0
		? visibleWidthPx / std::max(1, static_cast<int>(style.advance))
		: 0;
	if(visibleChars <= 0 && visibleWidthPx > 0){
		visibleChars = 1;
	}
	if(visibleChars > srcLen){
		visibleChars = srcLen;
	}
	const int scrollChars = (visibleChars > 0 && srcLen > visibleChars)
		? (srcLen - visibleChars)
		: 0;
	const int displayLen = srcLen - scrollChars;

	const char * displayText = src + scrollChars;
	if(opts.password){
		char * buf = AllocMaskBuffer();
		if(buf){
			for(int i = 0; i < displayLen; i++) buf[i] = '*';
			buf[displayLen] = '\0';
			displayText = buf;
		}
	}

	TextEffect effect = ResolveTextEffect(TextTone::Default, opts.effect);
	Uint8 brightness = opts.inactive ? static_cast<Uint8>(64) : effect.Brightness();
	bool  showCaret  = !opts.inactive && opts.showCaret;
	Uint8 caretHeight =
		static_cast<Uint8>(std::min(static_cast<int>(opts.heightPx) * 4 / 5,
		                            static_cast<int>(TextLineHeight(opts.textSize))));

	auto * payload = AllocPayload();
	if(payload){
		payload->text        = displayText;
		payload->textLen     = static_cast<Uint16>(displayLen);
		payload->textSize    = static_cast<Uint8>(opts.textSize);
		payload->effectColor = effect.EffectColor();
		payload->brightness  = brightness;
		payload->caretColor  = opts.caretColor;
		payload->caretHeight = caretHeight;
		payload->showCaret   = showCaret;
	}
	auto * ccd = AllocCustomData(
		silencer::clay_bridge::CustomKind::TextInput, payload);

	const float w = static_cast<float>(opts.widthPx);
	const float h = static_cast<float>(opts.heightPx);
	const uint16_t contentInsetX = std::min(opts.contentInsetX, opts.widthPx);
	const uint16_t contentInsetY = std::min(opts.contentInsetY, opts.heightPx);
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h) },
	           .padding = { contentInsetX, 0, contentInsetY, 0 },
	           .childGap = 0,
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		bool hovered = ::Clay_Hovered();
		if(handle.hoveredOut) *handle.hoveredOut = hovered;
		RegisterTextInputWidget(id, src, opts, handle);
		const float textW = static_cast<float>(
			std::max(1, static_cast<int>(opts.widthPx) - static_cast<int>(contentInsetX)));
		const float textH = static_cast<float>(style.lineHeight);
		CLAY({ .id = CLAY_SIDI(id, 1),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(textW), CLAY_SIZING_FIXED(textH) },
		       },
		       .custom = { .customData = ccd } }) {}
	}
}

}  // namespace silencer::ui::primitives
