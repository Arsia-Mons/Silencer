#ifndef SILENCER_UI_CLAY_PRIMITIVES_TEXT_INPUT_H
#define SILENCER_UI_CLAY_PRIMITIVES_TEXT_INPUT_H

// Screen-agnostic Clay primitive for paletted text input fields.
//
// The outer Clay element owns the clickable field bounds. The rendered text is
// an inner custom element inset horizontally by `contentInsetX`, vertically
// centered inside the field using stable bitmap-font metrics, and automatically
// tailed when the value exceeds the field width so the caret stays visible and
// older content is hidden on the left.
//
// The primitive owns no state and references no lobby/world/Config:
//
//   • The caller owns the character buffer and mutates it on
//     SDL_TEXTINPUT / SDL_KEYDOWN. The primitive just reads it.
//   • The caller pre-resolves `showCaret` (focus AND blink phase). The
//     legacy renderer blinks on `state_i % 32 < 16` — that's
//     screen-side bookkeeping now.
//
// `password` masks the rendered glyphs to '*'. `inactive` overrides
// `brightness` to 64 (matches legacy effectbrightness handling) and
// suppresses the caret. `numbersOnly` is a registry input-filter hint; the
// primitive ignores it for rendering. Enter handling is screen/controller-owned through
// UiInteractionRegistry; the primitive itself does not route SDL events.
//
// Memory: each call may allocate a small per-frame TextInputPayload + a
// ClayCustomData header + up to 256 chars of mask buffer from fixed-
// capacity bump arenas. Callers MUST invoke `TextInputBeginFrame()` once
// per layout pass before `Clay_BeginLayout` to reset them.

#include "clay/clay.h"
#include "primitives/text.h"
#include "shared.h"

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::ui::primitives {

struct TextInputOpts {
	Uint16 widthPx     = 90;    // bbox width (legacy `width`).
	Uint16 heightPx    = 19;    // bbox height (legacy `height`).
	TextSize textSize   = TextSize::Body;
	bool   password    = false; // render text as '*' chars.
	bool   numbersOnly = false; // screen-side input filter hint; primitive ignores.
	bool   inactive    = false; // dims to brightness 64; suppresses caret.
	TextEffect effect   = TextEffect::Default();
	Uint8  caretColor  = 140;   // legacy default.
	bool   showCaret   = false; // caller pre-resolves blink AND focus.
	Uint16 contentInsetX = 0;   // left inset from clickable field to text.
};

struct TextInputHandle {
	bool * hoveredOut = nullptr;  // Optional. Written each frame if non-null.
	const char * actionId = nullptr;
	const char * label = nullptr;
	UiInteractionRegistry * interactions = nullptr;
	int uid = -1;
	int maxLength = 0;
	bool cancelOnEscape = false;
};

// Resets the per-frame payload + custom-data + mask-buffer arenas. Call
// once before each Clay_BeginLayout.
void TextInputBeginFrame();

// Emits one TextInput subtree. Must be called inside an open CLAY parent
// element scope (or at the layout root). `id` is the Clay element's
// stable ID; supply a unique value per input on the screen. `text` must
// remain valid until Clay_EndLayout — Clay does not copy strings, and
// the mask path snapshots into a per-frame buffer.
void TextInput(Clay_String id,
               const char * text,
               TextInputOpts opts = {},
               TextInputHandle handle = {});

}  // namespace silencer::ui::primitives

#endif
