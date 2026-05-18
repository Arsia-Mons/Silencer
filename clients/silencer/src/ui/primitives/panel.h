#ifndef SILENCER_UI_CLAY_PRIMITIVES_PANEL_H
#define SILENCER_UI_CLAY_PRIMITIVES_PANEL_H

// Screen-agnostic Clay primitive for a sized panel container.
//
// Two chrome variants:
//
//   Variant       What it emits
//   LeftBare      An empty sized CLAY block. No chrome sprite is emitted —
//                 useful when the surrounding screen paints its own panel
//                 backdrop (e.g. baked into a background sprite).
//   RightChrome   A sized CLAY block with `.image = PackImage(bank, index)`
//                 — the bridge blits the chrome sprite at the bbox
//                 top-left using the sprite's natural size, painting the
//                 panel's decorative frame. Callers MUST specify
//                 `chromeBank` and `chromeIndex`; there is no default.
//
// In both variants, if `opts.title.length > 0` the primitive emits text
// inside the panel at
// (opts.titlePadLeft, opts.titlePadTop) padding offset from the top-left.
//
// The primitive owns no state and references no lobby/world/Config. State
// (which variant, where, what title) is supplied by the caller. Variants
// are an explicit enum. Sizing and chrome sprite identity are required
// parameters with no built-in defaults — defaults would silently bake a
// specific screen's aesthetic into "the panel primitive."
//
// Memory: no per-frame arena. The title goes through Text's own frame arena.

#include "clay/clay.h"
#include "shared.h"
#include "primitives/text.h"

namespace silencer::ui::primitives {

enum class PanelVariant : Uint8 {
	LeftBare,      // No chrome sprite emitted (background-baked).
	RightChrome,   // Bank 7 idx 8 sprite chrome (default).
};

struct PanelOpts {
	// Sizing — required. No default; any non-zero default would just
	// be one screen's panel dimensions wearing a "generic" label.
	Uint16          width        = 0;
	Uint16          height       = 0;
	// Chrome sprite identity — required when variant == RightChrome.
	// Defaulting to a specific bank/index silently couples the
	// primitive to whichever screen first used it; callers fill these.
	Uint8           chromeBank   = 0;
	Uint16          chromeIndex  = 0;
	// Title — empty string means no title is emitted (generic default).
	Clay_String     title        = { false, 0, nullptr };
	TextOpts        titleStyle   = { .size = TextSize::Heading };
	Uint16          titlePadLeft = 0;       // generic — caller positions title.
	Uint16          titlePadTop  = 0;       // generic — caller positions title.
};

// Emits one Panel subtree. Must be called inside an open CLAY parent
// element scope (or at the layout root). `id` is the panel's stable Clay
// ID; the title string must remain valid until Clay_EndLayout if non-empty.
void Panel(Clay_String id,
           PanelVariant variant,
           PanelOpts opts = {});

}  // namespace silencer::ui::primitives

#endif
