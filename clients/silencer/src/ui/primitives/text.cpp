#include "primitives/text.h"

#include "clay_ui_payloads.h"
#include "primitives/text_internal.h"

namespace silencer::ui::primitives {

namespace {

constexpr int kTextUserDataCapacity = 1024;
silencer::clay_bridge::TextDrawData g_userDataArena[kTextUserDataCapacity];
int g_userDataCount = 0;

Clay_TextElementConfigWrapMode ClayWrapMode(TextWrap wrap) {
	switch(wrap){
		case TextWrap::Words: return CLAY_TEXT_WRAP_WORDS;
		case TextWrap::Newlines: return CLAY_TEXT_WRAP_NEWLINES;
		case TextWrap::None: return CLAY_TEXT_WRAP_NONE;
	}
	return CLAY_TEXT_WRAP_WORDS;
}

Clay_TextAlignment ClayAlignment(TextAlign align) {
	switch(align){
		case TextAlign::Left: return CLAY_TEXT_ALIGN_LEFT;
		case TextAlign::Center: return CLAY_TEXT_ALIGN_CENTER;
		case TextAlign::Right: return CLAY_TEXT_ALIGN_RIGHT;
	}
	return CLAY_TEXT_ALIGN_LEFT;
}

bool DrawDataIsDefault(TextEffect effect,
                       text_internal::InternalTextOpts internalOpts) {
	return effect.Brightness() == 128 &&
	       !effect.ColorRamp() &&
	       !effect.DrawAlpha() &&
	       !internalOpts.measureInk;
}

silencer::clay_bridge::TextDrawData *
AllocUserData(TextEffect effect, text_internal::InternalTextOpts internalOpts) {
	if(g_userDataCount >= kTextUserDataCapacity) return nullptr;
	auto * slot = &g_userDataArena[g_userDataCount++];
	slot->brightness = effect.Brightness();
	slot->colorRamp = effect.ColorRamp();
	slot->drawAlpha = effect.DrawAlpha();
	slot->measureInk = internalOpts.measureInk;
	return slot;
}

}  // namespace

void TextBeginFrame() {
	g_userDataCount = 0;
}

TextEffect ResolveTextEffect(TextTone tone, TextEffect effect) {
	if(effect.HasOverride()) return effect;
	switch(tone){
		case TextTone::Default:  return TextEffect::LegacyPalette(0);
		case TextTone::Muted:    return TextEffect::LegacyPalette(0, 96);
		case TextTone::Accent:   return TextEffect::LegacyPalette(152);
		case TextTone::Warning:  return TextEffect::LegacyPalette(202);
		case TextTone::Danger:   return TextEffect::LegacyPalette(153);
		case TextTone::Disabled: return TextEffect::LegacyPalette(0, 64);
	}
	return TextEffect::LegacyPalette(0);
}

TextMetrics ResolveTextMetrics(TextSize size) {
	const auto style = text_internal::ResolveTextRenderStyle(size);
	return TextMetrics{ style.advance, style.lineHeight };
}

Uint16 TextAdvance(TextSize size) {
	return ResolveTextMetrics(size).advance;
}

Uint16 TextLineHeight(TextSize size) {
	return ResolveTextMetrics(size).lineHeight;
}

Clay_Dimensions MeasureText(Clay_String text, TextSize size) {
	TextMetrics metrics = ResolveTextMetrics(size);
	int32_t length = text.length > 0 ? text.length : 0;
	return Clay_Dimensions{
		static_cast<float>(length * metrics.advance),
		static_cast<float>(metrics.lineHeight),
	};
}

void Text(Clay_String text, TextOpts opts) {
	text_internal::TextWithInternalOptions(text, opts);
}

namespace text_internal {

TextRenderStyle ResolveTextRenderStyle(TextSize size) {
	switch(size){
		case TextSize::Title:           return {135, 11, 19};
		case TextSize::Heading:         return {134, 8, 15};
		case TextSize::Body:            return {133, 6, 11};
		case TextSize::BodySm:          return {133, 7, 11};
		case TextSize::Tiny:            return {132, 4, 7};
		case TextSize::HudCounter:      return {135, 12, 19};
		case TextSize::ScreenTitle:     return {135, 12, 19};
		case TextSize::TinyCounter:     return {132, 6, 7};
		case TextSize::MessageHeading:  return {134, 10, 15};
		case TextSize::MessageTitle:    return {136, 25, 23};
		case TextSize::MessageSubtitle: return {135, 13, 19};
		case TextSize::Prompt:          return {136, 16, 23};
		case TextSize::FieldLarge:      return {135, 9, 19};
		case TextSize::Footer:          return {133, 11, 11};
	}
	return {133, 6, 11};
}

void TextWithInternalOptions(Clay_String text,
                             TextOpts opts,
                             InternalTextOpts internalOpts) {
	const TextRenderStyle style = ResolveTextRenderStyle(opts.size);
	const TextEffect effect = ResolveTextEffect(opts.tone, opts.effect);
	void * userData = DrawDataIsDefault(effect, internalOpts)
		? nullptr
		: static_cast<void *>(AllocUserData(effect, internalOpts));

	CLAY_TEXT(text,
	          CLAY_TEXT_CONFIG({
	              .userData = userData,
	              .textColor = { static_cast<float>(effect.EffectColor()),
	                             0.0f, 0.0f, 255.0f },
	              .fontId = style.bank,
	              .fontSize = style.advance,
	              .letterSpacing = 0,
	              .lineHeight = style.lineHeight,
	              .wrapMode = ClayWrapMode(opts.wrap),
	              .textAlignment = ClayAlignment(opts.align),
	          }));
}

}  // namespace text_internal

}  // namespace silencer::ui::primitives
