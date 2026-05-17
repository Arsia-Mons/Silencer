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
// Bit 25 selects stretch: scale x/y independently to fill the element box.
constexpr std::uintptr_t kImageContainBit = static_cast<std::uintptr_t>(1) << 24;
constexpr std::uintptr_t kImageStretchBit = static_cast<std::uintptr_t>(1) << 25;

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

inline void * PackImageStretch(Uint8 bank, Uint16 index) {
	std::uintptr_t v = (static_cast<std::uintptr_t>(bank) << 16) |
	                   static_cast<std::uintptr_t>(index) | kImageStretchBit;
	return reinterpret_cast<void *>(v);
}

// Optional userData payload for Clay TEXT commands: extra effects beyond the
// textColor tint Clay already passes through.
struct TextDrawData {
	Uint8 brightness;     // 128 = neutral.
	bool colorRamp;       // True -> EffectRampColor instead of EffectColor.
	bool drawAlpha;       // True -> DrawAlphaed glyph blit.
	bool measureInk;      // True -> Clay layout uses visible glyph bounds.
};

// CUSTOM render command payload. Tag the kind so the compositor can dispatch.
enum class CustomKind : Uint8 {
	None = 0,
	ButtonSprite,
	ButtonNineSlice,
	ToggleSprite,
	ScrollBar,
	TextInput,
	BoxStroke,
	Sprite,
	TeamEmblem,
	Surface,
};

struct ClayCustomData {
	CustomKind kind;
	void * payload;
};

struct ButtonSpritePayload {
	Uint8  bank;
	Uint16 index;
	Uint8  brightness;
};

struct ButtonNineSlicePayload {
	Uint8  bank;
	Uint16 index;
	Uint8  brightness;
	Uint8  leftCap;
	Uint8  rightCap;
	Uint8  topCap;
	Uint8  bottomCap;
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
	Sint16 dstOffsetX;
	Sint16 dstOffsetY;

	SpritePayload(Uint8 bank = 0, Uint16 index = 0,
	              Sint16 srcX = 0, Sint16 srcY = 0,
	              Sint16 srcW = 0, Sint16 srcH = 0,
	              Uint8 effectColor = 0, Uint8 brightness = 128,
	              Uint8 rampColor = 0, Uint8 rampPlus = 0,
	              Sint16 dstOffsetX = 0, Sint16 dstOffsetY = 0)
		: bank(bank),
		  index(index),
		  srcX(srcX),
		  srcY(srcY),
		  srcW(srcW),
		  srcH(srcH),
		  effectColor(effectColor),
		  brightness(brightness),
		  rampColor(rampColor),
		  rampPlus(rampPlus),
		  dstOffsetX(dstOffsetX),
		  dstOffsetY(dstOffsetY) {}
};

struct SurfacePayload {
	const Uint8 * pixels = nullptr;
	Uint16 width = 0;
	Uint16 height = 0;
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
	Uint8        textSize;
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
	Uint8 topLeftCorner;
	Uint8 topRightCorner;
	Uint8 bottomRightCorner;
	Uint8 bottomLeftCorner;
};

}  // namespace silencer::clay_bridge

#endif
