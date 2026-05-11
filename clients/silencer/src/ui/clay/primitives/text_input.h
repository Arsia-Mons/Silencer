#ifndef SILENCER_UI_CLAY_PRIMITIVES_TEXT_INPUT_H
#define SILENCER_UI_CLAY_PRIMITIVES_TEXT_INPUT_H

// Screen-agnostic Clay primitive for paletted bank-font text input fields.
//
// Pixel-identical visual to the legacy `TextInput` widget (renderer.cpp
// `DrawTextInput`): DrawText at the bbox top-left + an optional 1-px
// vertical caret bar at (textLen * fontWidth, -1) of the bbox.
//
// The primitive owns no state and references no lobby/world/Config:
//
//   • The caller owns the character buffer and mutates it on
//     SDL_TEXTINPUT / SDL_KEYDOWN. The primitive just reads it.
//   • The caller pre-resolves `showCaret` (focus AND blink phase). The
//     legacy renderer blinks on `state_i % 32 < 16` — that's
//     screen-side bookkeeping now.
//   • The caller pre-slices `text` for horizontal scrolling — pass
//     `buffer + scrolled` if you've implemented scroll-on-overflow.
//
// `password` masks the rendered glyphs to '*'. `inactive` overrides
// `brightness` to 64 (matches legacy effectbrightness handling) and
// suppresses the caret. `numbersOnly` is documented here but is purely
// a screen-side input-filter hint — the primitive ignores it for
// rendering. `onEnter` in the handle is fired by the screen via
// `TextInputDispatchKey(handle, '\n')` when SDL reports Enter; the
// primitive itself does not route SDL events.
//
// Memory: each call may allocate a small per-frame TextInputPayload + a
// ClayCustomData header + up to 256 chars of mask buffer from fixed-
// capacity bump arenas. Callers MUST invoke `TextInputBeginFrame()` once
// per layout pass before `Clay_BeginLayout` to reset them.

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

struct TextInputOpts {
	Uint16 widthPx     = 90;    // bbox width (legacy `width`).
	Uint16 heightPx    = 19;    // bbox height (legacy `height`).
	Uint8  fontBank;            // caller-supplied; the primitive doesn't
	Uint8  fontWidth;           // know which bank/cell width is canonical.
	bool   password    = false; // render text as '*' chars.
	bool   numbersOnly = false; // screen-side input filter hint; primitive ignores.
	bool   inactive    = false; // dims to brightness 64; suppresses caret.
	Uint8  effectColor = 0;     // text tint palette index.
	Uint8  brightness  = 128;   // legacy default.
	Uint8  caretColor  = 140;   // legacy default.
	bool   showCaret   = false; // caller pre-resolves blink AND focus.
};

using TextInputEnterFn = void (*)(void * user);

struct TextInputHandle {
	bool *           hoveredOut;  // Optional. Written each frame if non-null.
	TextInputEnterFn onEnter;     // Optional. Fired by TextInputDispatchKey('\n').
	void *           user;        // Forwarded to onEnter.
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

// Screen-side dispatch helper. Routes a typed character to the input's
// onEnter callback for '\n' / '\r'. Returns true if the key was
// consumed (Enter), false otherwise (caller appends to the buffer).
// Lives next to the primitive because every screen needs the same
// "Enter fires onEnter, everything else passes through" routing.
bool TextInputDispatchKey(const TextInputHandle & handle, char ch);

}  // namespace silencer::ui::primitives

#endif
