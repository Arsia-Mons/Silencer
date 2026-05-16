#ifndef SILENCER_RENDER_CLAY_UI_PAYLOADS_H
#define SILENCER_RENDER_CLAY_UI_PAYLOADS_H

#include "shared.h"

#include <cstdint>

namespace silencer::clay_bridge {

// Pack a (bank, index) pair into a void* for Clay_ImageElementConfig.imageData.
// Bank occupies bits 16..23, index bits 0..15. Bit 24 selects how the
// compositor fits the sprite into its element box: clear = cover (scale to
// fill the box, preserving aspect, cropping overflow — CSS background-size:
// cover, for full-bleed backgrounds); set = contain (scale to fit inside the
// box, preserving aspect, no crop — for discrete graphics like the logo).
constexpr std::uintptr_t kImageContainBit = static_cast<std::uintptr_t>(1) << 24;

inline void * PackImage(Uint8 bank, Uint16 index) {
	std::uintptr_t v = (static_cast<std::uintptr_t>(bank) << 16) |
	                   static_cast<std::uintptr_t>(index);
	return reinterpret_cast<void *>(v);
}

inline void * PackImageContain(Uint8 bank, Uint16 index) {
	std::uintptr_t v = (static_cast<std::uintptr_t>(bank) << 16) |
	                   static_cast<std::uintptr_t>(index) | kImageContainBit;
	return reinterpret_cast<void *>(v);
}

// Optional userData payload for CLAY_TEXT: extra bank-text effects beyond the
// textColor tint Clay already passes through.
struct BankTextDrawData {
	Uint8 brightness;     // 128 = neutral.
	bool colorRamp;       // True -> EffectRampColor instead of EffectColor.
	bool drawAlpha;       // True -> DrawAlphaed glyph blit.
};

// CUSTOM render command payload. Tag the kind so the compositor can dispatch.
enum class CustomKind : Uint8 {
	None = 0,
	BankButtonChrome,
	ToggleSprite,
	ScrollBar,
	TextInput,
	BoxStroke,
	Sprite,
	TeamEmblem,
};

struct ClayCustomData {
	CustomKind kind;
	void * payload;
};

struct BankButtonChromePayload {
	Uint8  bank;
	Uint16 index;
	Uint8  brightness;
};

struct TogglePayload {
	Uint8  bank;
	Uint16 index;
	Uint8  effectColor;
	Uint8  brightness;
};

struct SpritePayload {
	Uint8  bank;
	Uint16 index;
	Sint16 srcX;
	Sint16 srcY;
	Sint16 srcW;
	Sint16 srcH;
	Uint8  effectColor;
	Uint8  brightness;
	Uint8  rampColor;
	Uint8  rampPlus;
};

struct TeamEmblemPayload {
	Uint8  bank;
	Uint16 index;
	Uint8  teamColor;
	Uint8  outlineColor;
	bool   scaled;
};

struct ScrollBarPayload {
	Uint8  bank;
	Uint16 trackIndex;
	Uint16 thumbIndex;
	Uint16 scrollPosition;
	Uint16 scrollMax;
};

struct TextInputPayload {
	const char * text;
	Uint16       textLen;
	Uint8        bank;
	Uint8        fontWidth;
	Uint8        effectColor;
	Uint8        brightness;
	Uint8        caretColor;
	Uint8        caretHeight;
	bool         showCaret;
};

struct BoxStrokePayload {
	Uint8 strokeColor;
	Uint8 strokeWidth;
	Uint8 outerHaloColor;
	Uint8 outerHaloWidth;
	Uint8 innerHaloColor;
	Uint8 innerHaloWidth;
	Uint8 haloOpacity;
	Uint8 sides;
};

}  // namespace silencer::clay_bridge

#endif
