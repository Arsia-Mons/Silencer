#ifndef SILENCER_UI_PRIMITIVES_TEXT_H
#define SILENCER_UI_PRIMITIVES_TEXT_H

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

enum class TextSize : Uint8 {
	Title,
	Heading,
	Body,
	BodySm,
	Tiny,
	HudCounter,
	ScreenTitle,
	TinyCounter,
	MessageHeading,
	MessageTitle,
	MessageSubtitle,
	Prompt,
	FieldLarge,
	Footer,
};

enum class TextTone : Uint8 {
	Default,
	Muted,
	Accent,
	Warning,
	Danger,
	Disabled,
};

enum class TextAlign : Uint8 {
	Left,
	Center,
	Right,
};

enum class TextWrap : Uint8 {
	Words,
	Newlines,
	None,
};

// Explicit bridge for legacy palette effects while the bitmap renderer is
// still paletted. Geometry callers should use TextSize metrics instead.
class TextEffect {
public:
	constexpr TextEffect() = default;

	static constexpr TextEffect Default() {
		return TextEffect{};
	}

	static constexpr TextEffect LegacyPalette(Uint8 effectColor,
	                                          Uint8 brightness = 128,
	                                          bool colorRamp = false,
	                                          bool drawAlpha = false) {
		return TextEffect(true, effectColor, brightness, colorRamp, drawAlpha);
	}

	constexpr bool HasOverride() const { return override_; }
	constexpr Uint8 EffectColor() const { return effectColor_; }
	constexpr Uint8 Brightness() const { return brightness_; }
	constexpr bool ColorRamp() const { return colorRamp_; }
	constexpr bool DrawAlpha() const { return drawAlpha_; }

private:
	constexpr TextEffect(bool overrideValue,
	                     Uint8 effectColor,
	                     Uint8 brightness,
	                     bool colorRamp,
	                     bool drawAlpha)
		: override_(overrideValue),
		  effectColor_(effectColor),
		  brightness_(brightness),
		  colorRamp_(colorRamp),
		  drawAlpha_(drawAlpha) {}

	bool override_ = false;
	Uint8 effectColor_ = 0;
	Uint8 brightness_ = 128;
	bool colorRamp_ = false;
	bool drawAlpha_ = false;
};

struct TextOpts {
	TextSize size = TextSize::Body;
	TextTone tone = TextTone::Default;
	TextAlign align = TextAlign::Left;
	TextWrap wrap = TextWrap::Words;
	TextEffect effect = TextEffect::Default();
};

using TextStyle = TextOpts;

struct TextMetrics {
	Uint16 advance;
	Uint16 lineHeight;
};

void TextBeginFrame();

void Text(Clay_String text, TextOpts opts = TextOpts{});

TextMetrics ResolveTextMetrics(TextSize size);
Uint16 TextAdvance(TextSize size);
Uint16 TextLineHeight(TextSize size);
Clay_Dimensions MeasureText(Clay_String text, TextSize size);

TextEffect ResolveTextEffect(TextTone tone, TextEffect effect);

}  // namespace silencer::ui::primitives

#endif
