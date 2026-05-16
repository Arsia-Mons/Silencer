#include "button.h"

#include "bank_text.h"
#include "clay_ui_payloads.h"
#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace silencer::ui::primitives {

namespace {

constexpr int kSpritePayloadCapacity = 512;
silencer::clay_bridge::ButtonSpritePayload g_spritePayloads[kSpritePayloadCapacity];
int g_spritePayloadCount = 0;

constexpr int kNineSlicePayloadCapacity = 128;
silencer::clay_bridge::ButtonNineSlicePayload g_nineSlicePayloads[kNineSlicePayloadCapacity];
int g_nineSlicePayloadCount = 0;

constexpr int kCustomDataCapacity = 640;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

constexpr int kStringArenaCapacity = 32768;
char g_stringArena[kStringArenaCapacity];
int g_stringArenaOffset = 0;

struct ButtonLines {
	Clay_String lines[32];
	int count = 0;
	int maxChars = 0;
};

struct ResolvedButton {
	bool hasSprite = false;
	bool nineSlice = false;
	Uint8 spriteBank = 0;
	Uint16 spriteIndex = 0;
	int fixedWidth = 0;
	int fixedHeight = 0;
	BankTextVariant textVariant = BankTextVariant::BodySm;
	int cellWidth = 7;
	int textHeight = 11;
	int yOffset = 0;
	int xNudge = 0;
	int defaultPaddingX = 0;
	int defaultPaddingY = 0;
};

Clay_String EmptyString() {
	static const char kEmpty[] = "";
	return Clay_String{ false, 0, kEmpty };
}

Clay_String CopyString(const char * text, int length) {
	if(!text || length <= 0) return EmptyString();
	if(length + 1 > kStringArenaCapacity - g_stringArenaOffset){
		return Clay_String{ false, length, text };
	}
	char * dst = &g_stringArena[g_stringArenaOffset];
	std::memcpy(dst, text, static_cast<size_t>(length));
	dst[length] = '\0';
	g_stringArenaOffset += length + 1;
	return Clay_String{ false, length, dst };
}

Clay_String CopyString(Clay_String text) {
	return CopyString(text.chars, std::max(0, static_cast<int>(text.length)));
}

std::string ToStd(Clay_String text) {
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(std::max(0, text.length)));
}

int TextHeightFor(BankTextVariant variant) {
	switch(variant){
		case BankTextVariant::Title: return 19;
		case BankTextVariant::Heading: return 15;
		case BankTextVariant::Body:
		case BankTextVariant::BodySm:
			return 11;
	}
	return 11;
}

int TextCellWidthFor(BankTextVariant variant) {
	switch(variant){
		case BankTextVariant::Title: return 11;
		case BankTextVariant::Heading: return 8;
		case BankTextVariant::Body: return 6;
		case BankTextVariant::BodySm: return 7;
	}
	return 7;
}

ResolvedButton ResolveButton(const ButtonOpts& opts) {
	ResolvedButton out;
	switch(opts.variant){
		case ButtonVariant::Oval:
			out.hasSprite = true;
			out.spriteBank = 6;
			out.spriteIndex = 7;
			out.fixedWidth = 196;
			out.fixedHeight = 33;
			out.textVariant = BankTextVariant::Title;
			out.yOffset = 8;
			switch(opts.size){
				case ButtonSize::Sm:
				case ButtonSize::Compact:
					out.spriteIndex = 28;
					out.fixedWidth = 112;
					break;
				case ButtonSize::Lg:
					out.spriteIndex = 23;
					out.fixedWidth = 220;
					break;
				case ButtonSize::Auto:
					out.spriteIndex = 7;
					out.fixedWidth = 0;
					out.fixedHeight = 0;
					out.nineSlice = true;
					out.defaultPaddingX = 24;
					out.defaultPaddingY = 7;
					break;
				case ButtonSize::Md:
				default:
					break;
			}
			break;
		case ButtonVariant::Chrome:
			out.hasSprite = true;
			out.spriteBank = 7;
			out.spriteIndex = 24;
			out.fixedWidth = 156;
			out.fixedHeight = 21;
			out.textVariant = BankTextVariant::Heading;
			out.yOffset = 4;
			break;
		case ButtonVariant::Text:
			out.textVariant = BankTextVariant::BodySm;
			out.defaultPaddingX = 0;
			out.defaultPaddingY = 0;
			if(opts.size == ButtonSize::Compact){
				out.fixedWidth = 52;
				out.fixedHeight = 21;
				out.yOffset = 8;
				out.xNudge = 1;
			}
			break;
		case ButtonVariant::Ghost:
			out.textVariant = BankTextVariant::Heading;
			out.defaultPaddingX = 0;
			out.defaultPaddingY = 0;
			break;
	}
	out.cellWidth = TextCellWidthFor(out.textVariant);
	out.textHeight = TextHeightFor(out.textVariant);
	return out;
}

void AddLine(ButtonLines& out, const char * text, int length) {
	if(out.count >= static_cast<int>(sizeof(out.lines) / sizeof(out.lines[0]))) return;
	if(length < 0) length = 0;
	Clay_String line = CopyString(text, length);
	out.lines[out.count++] = line;
	if(length > out.maxChars) out.maxChars = length;
}

void AddWrappedLine(ButtonLines& out, const char * text, int length, int maxChars) {
	if(length <= 0){
		AddLine(out, text, 0);
		return;
	}
	if(maxChars <= 0 || length <= maxChars){
		AddLine(out, text, length);
		return;
	}

	int start = 0;
	while(start < length){
		while(start < length && (text[start] == ' ' || text[start] == '\t')) start++;
		if(start >= length) break;
		int remaining = length - start;
		if(remaining <= maxChars){
			AddLine(out, text + start, remaining);
			break;
		}
		int end = start + maxChars;
		int breakAt = -1;
		for(int i = end; i > start; --i){
			if(text[i - 1] == ' ' || text[i - 1] == '\t'){
				breakAt = i - 1;
				break;
			}
		}
		if(breakAt <= start){
			AddLine(out, text + start, maxChars);
			start += maxChars;
		}else{
			AddLine(out, text + start, breakAt - start);
			start = breakAt + 1;
		}
	}
	if(out.count == 0) AddLine(out, text, 0);
}

ButtonLines BuildLines(Clay_String label, bool wrapText, int maxTextWidth, int cellWidth) {
	ButtonLines out;
	if(!label.chars || label.length <= 0){
		AddLine(out, "", 0);
		return out;
	}
	int maxChars = 0;
	if(wrapText && maxTextWidth > 0 && cellWidth > 0){
		maxChars = maxTextWidth / cellWidth;
		if(maxChars < 1) maxChars = 1;
	}
	const char * start = label.chars;
	int segmentStart = 0;
	for(int i = 0; i < label.length; ++i){
		if(label.chars[i] == '\n'){
			AddWrappedLine(out, start + segmentStart, i - segmentStart, maxChars);
			segmentStart = i + 1;
		}
	}
	AddWrappedLine(out, start + segmentStart, label.length - segmentStart, maxChars);
	return out;
}

silencer::clay_bridge::ButtonSpritePayload *
AllocSpritePayload(Uint8 bank, Uint16 index, Uint8 brightness) {
	if(g_spritePayloadCount >= kSpritePayloadCapacity) return nullptr;
	auto * p = &g_spritePayloads[g_spritePayloadCount++];
	p->bank = bank;
	p->index = index;
	p->brightness = brightness;
	return p;
}

silencer::clay_bridge::ButtonNineSlicePayload *
AllocNineSlicePayload(Uint8 bank, Uint16 index, Uint8 brightness) {
	if(g_nineSlicePayloadCount >= kNineSlicePayloadCapacity) return nullptr;
	auto * p = &g_nineSlicePayloads[g_nineSlicePayloadCount++];
	p->bank = bank;
	p->index = index;
	p->brightness = brightness;
	p->leftCap = 16;
	p->rightCap = 16;
	p->topCap = 16;
	p->bottomCap = 16;
	return p;
}

silencer::clay_bridge::ClayCustomData *
AllocCustomData(silencer::clay_bridge::CustomKind kind, void * payload) {
	if(g_customDataCount >= kCustomDataCapacity) return nullptr;
	auto * c = &g_customData[g_customDataCount++];
	c->kind = kind;
	c->payload = payload;
	return c;
}

int ClampAutoWidth(int width, const ButtonOpts& opts) {
	if(opts.maxWidth > 0 && width > opts.maxWidth) width = opts.maxWidth;
	if(opts.minWidth > 0 && width < opts.minWidth) width = opts.minWidth;
	return width < 1 ? 1 : width;
}

void RegisterButtonWidget(Clay_String id,
                          Clay_String label,
                          const ButtonOpts& opts,
                          ButtonHandle handle) {
	if(!handle.interactions || !handle.actionId || !*handle.actionId) return;
	silencer::ui::UiInteractable widget;
	widget.id = handle.actionId;
	widget.labelText = ToStd(label);
	widget.kind = UiInteractableKind::Button;
	widget.selected = opts.selected;
	widget.inactive = opts.disabled;
	widget.clayId = CLAY_SID(id);
	widget.hasClayId = true;
	handle.interactions->RegisterInteractable(widget);
}

void EmitButtonText(const ButtonLines& lines,
                    const ResolvedButton& resolved,
                    Uint8 effectColor,
                    Uint8 brightness) {
	for(int i = 0; i < lines.count; ++i){
		BankText(lines.lines[i],
		         resolved.textVariant,
		         { .effectColor = effectColor,
		           .brightness = brightness });
	}
}

}  // namespace

void ButtonBeginFrame() {
	g_spritePayloadCount = 0;
	g_nineSlicePayloadCount = 0;
	g_customDataCount = 0;
	g_stringArenaOffset = 0;
}

void Button(Clay_String id,
            Clay_String label,
            ButtonOpts opts,
            ButtonHandle handle) {
	Clay_String stableId = CopyString(id);
	Clay_String stableLabel = CopyString(label);
	ResolvedButton resolved = ResolveButton(opts);

	int paddingX = opts.paddingX > 0 ? opts.paddingX : resolved.defaultPaddingX;
	int paddingY = opts.paddingY > 0 ? opts.paddingY : resolved.defaultPaddingY;
	int maxTextWidth = 0;
	if(opts.wrapText && opts.maxWidth > 0){
		maxTextWidth = opts.maxWidth - paddingX * 2;
		if(maxTextWidth < resolved.cellWidth) maxTextWidth = resolved.cellWidth;
	}
	ButtonLines lines = BuildLines(stableLabel, opts.wrapText, maxTextWidth, resolved.cellWidth);

	const int contentWidth = lines.maxChars * resolved.cellWidth;
	const int contentHeight = std::max(1, lines.count) * resolved.textHeight;
	int width = resolved.fixedWidth > 0 ? resolved.fixedWidth
	                                    : contentWidth + paddingX * 2;
	int height = resolved.fixedHeight > 0 ? resolved.fixedHeight
	                                      : contentHeight + paddingY * 2;
	if(resolved.fixedWidth == 0) width = ClampAutoWidth(width, opts);
	if(resolved.nineSlice && height < 33) height = 33;
	if(height < 1) height = 1;

	const float boxW = static_cast<float>(width);
	const float boxH = static_cast<float>(height);

	if(resolved.hasSprite){
		void * payload = resolved.nineSlice
			? static_cast<void *>(AllocNineSlicePayload(resolved.spriteBank, resolved.spriteIndex, 128))
			: static_cast<void *>(AllocSpritePayload(resolved.spriteBank, resolved.spriteIndex, 128));
		auto * ccd = AllocCustomData(
			resolved.nineSlice ? silencer::clay_bridge::CustomKind::ButtonNineSlice
			                   : silencer::clay_bridge::CustomKind::ButtonSprite,
			payload);
		CLAY({ .id = CLAY_SID(stableId),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(boxW), CLAY_SIZING_FIXED(boxH) },
		           .padding = { static_cast<uint16_t>(paddingX),
		                        static_cast<uint16_t>(paddingX),
		                        static_cast<uint16_t>(resolved.nineSlice ? paddingY : resolved.yOffset),
		                        static_cast<uint16_t>(paddingY) },
		           .childGap = 0,
		           .childAlignment = { CLAY_ALIGN_X_CENTER,
		                               resolved.nineSlice ? CLAY_ALIGN_Y_CENTER
		                                                  : CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .custom = { .customData = ccd } }) {
			bool hovered = ::Clay_Hovered() && !opts.disabled;
			Uint8 brightness = hovered ? static_cast<Uint8>(136) : static_cast<Uint8>(128);
			if(resolved.nineSlice){
				auto * p = reinterpret_cast<silencer::clay_bridge::ButtonNineSlicePayload *>(payload);
				if(p) p->brightness = brightness;
			}else{
				auto * p = reinterpret_cast<silencer::clay_bridge::ButtonSpritePayload *>(payload);
				if(p) p->brightness = brightness;
			}
			if(handle.hoveredOut) *handle.hoveredOut = hovered;
			RegisterButtonWidget(stableId, stableLabel, opts, handle);
			EmitButtonText(lines, resolved, opts.effectColor, brightness);
		}
		return;
	}

	CLAY({ .id = CLAY_SID(stableId),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(boxW), CLAY_SIZING_FIXED(boxH) },
	           .padding = { static_cast<uint16_t>(paddingX + std::max(0, resolved.xNudge)),
	                        static_cast<uint16_t>(paddingX),
	                        static_cast<uint16_t>(resolved.fixedHeight > 0 ? resolved.yOffset : paddingY),
	                        static_cast<uint16_t>(paddingY) },
	           .childGap = 0,
	           .childAlignment = { CLAY_ALIGN_X_CENTER,
	                               resolved.fixedHeight > 0 ? CLAY_ALIGN_Y_TOP
	                                                        : CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		bool hovered = ::Clay_Hovered() && !opts.disabled;
		Uint8 brightness = hovered ? static_cast<Uint8>(136) : static_cast<Uint8>(128);
		if(handle.hoveredOut) *handle.hoveredOut = hovered;
		RegisterButtonWidget(stableId, stableLabel, opts, handle);
		EmitButtonText(lines, resolved, opts.effectColor, brightness);
	}
}

}  // namespace silencer::ui::primitives
