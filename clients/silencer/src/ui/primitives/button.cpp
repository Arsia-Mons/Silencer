#include "button.h"

#include "clay_ui_payloads.h"
#include "primitives/text.h"
#include "primitives/text_internal.h"
#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>

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

enum class ButtonVisualMode : Uint8 {
	Inactive,
	Activating,
	Active,
	Deactivating,
};

struct ButtonVisualState {
	ButtonVisualMode mode = ButtonVisualMode::Inactive;
	int phase = 0;
	int visiblePhase = 0;
	float accumulatedSeconds = 0.0f;
};

struct ButtonVisualFrame {
	int phase = 0;
	Uint8 brightness = 128;
};

std::unordered_map<uint32_t, ButtonVisualState> g_visualStates;
float g_visualDeltaSeconds = 1.0f / 24.0f;
float g_visualStepSeconds = 1.0f / 24.0f;

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
	int minHeight = 0;
	TextSize textSize = TextSize::BodySm;
	TextMetrics textMetrics = ResolveTextMetrics(TextSize::BodySm);
	int yOffset = 0;
	int visualTextPaddingTop = 0;
	int defaultPaddingX = 0;
	int defaultPaddingY = 0;
	bool centerContentY = false;
	bool measureTextInk = false;
	Uint8 leftCap = 16;
	Uint8 rightCap = 16;
	Uint8 topCap = 16;
	Uint8 bottomCap = 16;
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

ResolvedButton ResolveButton(const ButtonOpts& opts) {
	ResolvedButton out;
	switch(opts.variant){
		case ButtonVariant::Oval:
			out.hasSprite = true;
			out.spriteBank = 6;
			out.spriteIndex = 7;
			out.fixedWidth = 196;
			out.fixedHeight = 33;
			out.textSize = TextSize::Title;
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
					out.minHeight = 33;
					out.nineSlice = true;
					out.defaultPaddingX = 24;
					out.defaultPaddingY = 7;
					out.centerContentY = true;
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
			out.textSize = TextSize::Heading;
			out.yOffset = 4;
			if(opts.size == ButtonSize::Auto){
				out.fixedWidth = 0;
				out.nineSlice = true;
				out.leftCap = 12;
				out.rightCap = 12;
				out.topCap = 4;
				out.bottomCap = 4;
			}
			break;
		case ButtonVariant::Text:
			out.textSize = TextSize::BodySm;
			out.defaultPaddingX = 0;
			out.defaultPaddingY = 0;
			out.measureTextInk = true;
			if(opts.size == ButtonSize::Compact){
				out.fixedWidth = 52;
				out.fixedHeight = 21;
				// B52x21 text-only buttons draw at y + 8 in the legacy
				// renderer. Clay centers the measured 11px bank-133 line box,
				// so bias the content area until the emitted text bbox lands
				// on that same 8px offset.
				out.visualTextPaddingTop = 6;
			}
			break;
		case ButtonVariant::Ghost:
			out.textSize = TextSize::Heading;
			out.defaultPaddingX = 0;
			out.defaultPaddingY = 0;
			break;
	}
	out.textMetrics = ResolveTextMetrics(out.textSize);
	return out;
}

int ClampPhase(int phase) {
	if(phase < 0) return 0;
	if(phase > 4) return 4;
	return phase;
}

ButtonVisualFrame FrameForPhase(int phase) {
	phase = ClampPhase(phase);
	return ButtonVisualFrame{
		phase,
		static_cast<Uint8>(128 + phase * 2),
	};
}

ButtonVisualFrame CurrentVisual(const ButtonVisualState& state) {
	return FrameForPhase(state.visiblePhase);
}

ButtonVisualFrame AdvanceVisualState(ButtonVisualState& state, bool targeted) {
	if(targeted){
		if(state.mode == ButtonVisualMode::Inactive){
			state.mode = ButtonVisualMode::Activating;
			state.phase = 0;
		}else if(state.mode == ButtonVisualMode::Deactivating){
			state.mode = ButtonVisualMode::Activating;
		}
	}else{
		if(state.mode == ButtonVisualMode::Active){
			state.mode = ButtonVisualMode::Deactivating;
			state.phase = 4;
		}else if(state.mode == ButtonVisualMode::Activating){
			state.mode = ButtonVisualMode::Deactivating;
		}
	}

	int phase = 0;
	switch(state.mode){
		case ButtonVisualMode::Inactive:
			phase = 0;
			break;
		case ButtonVisualMode::Activating:
		case ButtonVisualMode::Deactivating:
			phase = ClampPhase(state.phase);
			break;
		case ButtonVisualMode::Active:
			phase = 4;
			break;
	}
	state.visiblePhase = phase;

	if(state.mode == ButtonVisualMode::Activating){
		if(state.phase >= 4){
			state.mode = ButtonVisualMode::Active;
			state.phase = 4;
		}else{
			state.phase++;
		}
	}else if(state.mode == ButtonVisualMode::Deactivating){
		if(state.phase <= 0){
			state.mode = ButtonVisualMode::Inactive;
			state.phase = 0;
		}else{
			state.phase--;
		}
	}

	return FrameForPhase(phase);
}

bool IsStableForTarget(const ButtonVisualState& state, bool targeted) {
	return (targeted && state.mode == ButtonVisualMode::Active) ||
	       (!targeted && state.mode == ButtonVisualMode::Inactive);
}

ButtonVisualFrame StepVisualState(uint32_t clayId, bool targeted) {
	ButtonVisualState& state = g_visualStates[clayId];
	ButtonVisualFrame frame = CurrentVisual(state);
	if(IsStableForTarget(state, targeted)){
		state.accumulatedSeconds = 0.0f;
		return frame;
	}
	state.accumulatedSeconds += std::max(0.0f, g_visualDeltaSeconds);
	const float step = std::max(0.001f, g_visualStepSeconds);
	while(state.accumulatedSeconds + 0.0001f >= step){
		state.accumulatedSeconds -= step;
		frame = AdvanceVisualState(state, targeted);
	}
	if(IsStableForTarget(state, targeted)){
		state.accumulatedSeconds = 0.0f;
	}
	return frame;
}

Uint16 SpriteIndexForFrame(const ResolvedButton& resolved,
                           const ButtonOpts& opts,
                           int phase) {
	if(opts.variant == ButtonVariant::Oval){
		return static_cast<Uint16>(resolved.spriteIndex + ClampPhase(phase));
	}
	return resolved.spriteIndex;
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
AllocNineSlicePayload(Uint8 bank,
                      Uint16 index,
                      Uint8 brightness,
                      Uint8 leftCap,
                      Uint8 rightCap,
                      Uint8 topCap,
                      Uint8 bottomCap) {
	if(g_nineSlicePayloadCount >= kNineSlicePayloadCapacity) return nullptr;
	auto * p = &g_nineSlicePayloads[g_nineSlicePayloadCount++];
	p->bank = bank;
	p->index = index;
	p->brightness = brightness;
	p->leftCap = leftCap;
	p->rightCap = rightCap;
	p->topCap = topCap;
	p->bottomCap = bottomCap;
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

void RegisterButtonWidget(Clay_String label,
                          const ButtonOpts& opts,
                          ButtonHandle handle,
                          Clay_ElementId clayId) {
	if(!handle.interactions || !handle.actionId || !*handle.actionId) return;
	silencer::ui::UiInteractable widget;
	widget.id = handle.actionId;
	widget.labelText = ToStd(label);
	widget.kind = UiInteractableKind::Button;
	widget.selected = opts.selected;
	widget.inactive = opts.disabled;
	widget.clayId = clayId;
	widget.hasClayId = true;
	handle.interactions->RegisterInteractable(widget);
}

bool IsButtonFocused(ButtonHandle handle) {
	if(!handle.interactions || !handle.actionId || !*handle.actionId) return false;
	const silencer::ui::UiElementSnapshot * snapshot =
		handle.interactions->FindById(handle.actionId);
	return snapshot && snapshot->focused && snapshot->enabled;
}

void EmitButtonText(const ButtonLines& lines,
                    const ResolvedButton& resolved,
                    TextEffect baseEffect,
                    Uint8 brightness) {
	const TextEffect resolvedEffect = ResolveTextEffect(TextTone::Default, baseEffect);
	const TextEffect frameEffect = TextEffect::LegacyPalette(
		resolvedEffect.EffectColor(),
		brightness,
		resolvedEffect.ColorRamp(),
		resolvedEffect.DrawAlpha());
	for(int i = 0; i < lines.count; ++i){
		text_internal::TextWithInternalOptions(
			lines.lines[i],
			TextOpts{
				.size = resolved.textSize,
				.effect = frameEffect,
			},
			text_internal::InternalTextOpts{ .measureInk = resolved.measureTextInk });
	}
}

}  // namespace

void ButtonBeginFrame(float animationDeltaSeconds, float animationStepSeconds) {
	g_spritePayloadCount = 0;
	g_nineSlicePayloadCount = 0;
	g_customDataCount = 0;
	g_stringArenaOffset = 0;
	g_visualDeltaSeconds = std::max(0.0f, animationDeltaSeconds);
	g_visualStepSeconds = std::max(0.001f, animationStepSeconds);
}

struct ButtonVisualStateGuard::Snapshot {
	std::unordered_map<uint32_t, ButtonVisualState> visualStates;
	float visualDeltaSeconds = 1.0f / 24.0f;
	float visualStepSeconds = 1.0f / 24.0f;
};

ButtonVisualStateGuard::ButtonVisualStateGuard()
	: snapshot_(new Snapshot{
		g_visualStates,
		g_visualDeltaSeconds,
		g_visualStepSeconds,
	}) {
}

ButtonVisualStateGuard::~ButtonVisualStateGuard() {
	if(!snapshot_) return;
	g_visualStates = snapshot_->visualStates;
	g_visualDeltaSeconds = snapshot_->visualDeltaSeconds;
	g_visualStepSeconds = snapshot_->visualStepSeconds;
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
		if(maxTextWidth < resolved.textMetrics.advance) maxTextWidth = resolved.textMetrics.advance;
	}
	ButtonLines lines = BuildLines(stableLabel, opts.wrapText, maxTextWidth, resolved.textMetrics.advance);

	const int contentWidth = lines.maxChars * resolved.textMetrics.advance;
	const int contentHeight = std::max(1, lines.count) * resolved.textMetrics.lineHeight;
	int width = resolved.fixedWidth > 0 ? resolved.fixedWidth
	                                    : contentWidth + paddingX * 2;
	int height = resolved.fixedHeight > 0 ? resolved.fixedHeight
	                                      : contentHeight + paddingY * 2;
	if(opts.widthOverride > 0){
		width = opts.widthOverride;
	}else if(resolved.fixedWidth == 0){
		width = ClampAutoWidth(width, opts);
	}
	if(resolved.minHeight > 0 && height < resolved.minHeight) height = resolved.minHeight;
	if(height < 1) height = 1;

	const float boxW = static_cast<float>(width);
	const float boxH = static_cast<float>(height);
	const Clay_ElementId clayId = CLAY_SID(stableId);
	const Clay_LayoutAlignmentX alignX = opts.alignLeft
		? CLAY_ALIGN_X_LEFT
		: CLAY_ALIGN_X_CENTER;

	if(resolved.hasSprite){
		void * payload = resolved.nineSlice
			? static_cast<void *>(AllocNineSlicePayload(resolved.spriteBank,
			                                           resolved.spriteIndex,
			                                           128,
			                                           resolved.leftCap,
			                                           resolved.rightCap,
			                                           resolved.topCap,
			                                           resolved.bottomCap))
			: static_cast<void *>(AllocSpritePayload(resolved.spriteBank, resolved.spriteIndex, 128));
		auto * ccd = AllocCustomData(
			resolved.nineSlice ? silencer::clay_bridge::CustomKind::ButtonNineSlice
			                   : silencer::clay_bridge::CustomKind::ButtonSprite,
			payload);
		CLAY({ .id = clayId,
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(boxW), CLAY_SIZING_FIXED(boxH) },
		           .padding = { static_cast<uint16_t>(paddingX),
		                        static_cast<uint16_t>(paddingX),
		                        static_cast<uint16_t>(resolved.nineSlice && resolved.centerContentY
		                                               ? paddingY
		                                               : resolved.yOffset),
		                        static_cast<uint16_t>(paddingY) },
		           .childGap = 0,
		           .childAlignment = { alignX,
		                               resolved.nineSlice && resolved.centerContentY
		                                                  ? CLAY_ALIGN_Y_CENTER
		                                                  : CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .custom = { .customData = ccd } }) {
			bool hovered = ::Clay_Hovered() && !opts.disabled;
			RegisterButtonWidget(stableLabel, opts, handle, clayId);
			bool focused = !opts.disabled && IsButtonFocused(handle);
			ButtonVisualFrame visual = StepVisualState(
				clayId.id,
				!opts.disabled && (hovered || focused));
			Uint16 spriteIndex = SpriteIndexForFrame(resolved, opts, visual.phase);
			if(resolved.nineSlice){
				auto * p = reinterpret_cast<silencer::clay_bridge::ButtonNineSlicePayload *>(payload);
				if(p){
					p->index = spriteIndex;
					p->brightness = visual.brightness;
				}
			}else{
				auto * p = reinterpret_cast<silencer::clay_bridge::ButtonSpritePayload *>(payload);
				if(p){
					p->index = spriteIndex;
					p->brightness = visual.brightness;
				}
			}
			if(handle.hoveredOut) *handle.hoveredOut = hovered;
			EmitButtonText(lines, resolved, opts.textEffect, visual.brightness);
		}
		return;
	}

	CLAY({ .id = clayId,
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(boxW), CLAY_SIZING_FIXED(boxH) },
	           .padding = { static_cast<uint16_t>(paddingX),
	                        static_cast<uint16_t>(paddingX),
	                        static_cast<uint16_t>(paddingY + resolved.visualTextPaddingTop),
	                        static_cast<uint16_t>(paddingY) },
	           .childGap = 0,
	           .childAlignment = { alignX,
	                               CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		bool hovered = ::Clay_Hovered() && !opts.disabled;
		RegisterButtonWidget(stableLabel, opts, handle, clayId);
		bool focused = !opts.disabled && IsButtonFocused(handle);
		ButtonVisualFrame visual = StepVisualState(
			clayId.id,
			!opts.disabled && (hovered || focused));
		if(handle.hoveredOut) *handle.hoveredOut = hovered;
		EmitButtonText(lines, resolved, opts.textEffect, visual.brightness);
	}
}

}  // namespace silencer::ui::primitives
