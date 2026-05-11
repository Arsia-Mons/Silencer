#ifndef SILENCER_UI_CLAY_PRIMITIVES_PANEL_H
#define SILENCER_UI_CLAY_PRIMITIVES_PANEL_H

// Screen-agnostic Clay primitive for a sized panel container.
//
// Two chrome variants:
//
//   Variant       What it emits
//   LeftBare      An empty sized CLAY block. The visual chrome for left-side
//                 panels is baked into the lobby's full-screen background
//                 sprite, so the primitive only reserves the layout slot.
//   RightChrome   A sized CLAY block with `.image = PackImage(bank, index)`
//                 — the bridge blits the chrome sprite (default bank 7
//                 idx 8) at the bbox top-left using the sprite's natural
//                 size, painting the panel's decorative frame.
//
// In both variants, if `opts.title.length > 0` the primitive emits a
// BankText (default variant Heading) inside the panel at
// (opts.titlePadLeft, opts.titlePadTop) padding offset from the top-left.
//
// The primitive owns no state and references no lobby/world/Config. State
// (which variant, where, what title) is supplied by the caller. Variants
// are an explicit enum.
//
// Memory: no per-frame arena. The title goes through BankText's own arena
// (if non-default brightness/colorRamp is used), so callers MUST have
// invoked BankTextBeginFrame() before Clay_BeginLayout if they pass a
// title with non-default opts.

#include "clay/clay.h"
#include "shared.h"
#include "primitives/bank_text.h"

namespace silencer::ui::primitives {

enum class PanelVariant : Uint8 {
	LeftBare,      // No chrome sprite emitted (background-baked).
	RightChrome,   // Bank 7 idx 8 sprite chrome (default).
};

struct PanelOpts {
	Uint16          width        = 222;
	Uint16          height       = 390;
	Uint8           chromeBank   = 7;     // RightChrome only.
	Uint16          chromeIndex  = 8;     // RightChrome only.
	Clay_String     title        = { false, 0, nullptr };  // Empty = no title emitted.
	BankTextVariant titleVariant = BankTextVariant::Heading;
	Uint8           titleEffectColor = 0;
	Uint8           titleBrightness  = 128;
	Uint16          titlePadLeft = 2;
	Uint16          titlePadTop  = 0;
};

// Emits one Panel subtree. Must be called inside an open CLAY parent
// element scope (or at the layout root). `id` is the panel's stable Clay
// ID; the title string must remain valid until Clay_EndLayout if non-empty.
void Panel(Clay_String id,
           PanelVariant variant,
           PanelOpts opts = {});

}  // namespace silencer::ui::primitives

#endif
