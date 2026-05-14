#ifndef SILENCER_UI_CLAY_PRIMITIVES_TOGGLE_H
#define SILENCER_UI_CLAY_PRIMITIVES_TOGGLE_H

// Screen-agnostic Clay primitive for sprite-faced radio toggles.
//
// Mirrors the legacy `Toggle` widget's render contract without any of its
// group / "set" / state-owning baggage. The primitive emits a CUSTOM
// render command whose payload tells the bridge to blit
// `spritebank[bank][index]`, optionally applying an EffectColor tint and
// an EffectBrightness that swaps between two values keyed on `selected`.
//
// Mutually-exclusive group behavior is the CALLER's job:
//   - caller passes `selected` per toggle (true for one, false for the rest);
//   - caller passes a distinct `user` per toggle so its onClick callback
//     can identify which one was clicked.
// No global `set` integer, no shared state. The primitive references no
// lobby / world / Config types.
//
// Memory: each call allocates up to one ClickAdapter + one TogglePayload
// + one ClayCustomData header from fixed-capacity bump arenas. Callers
// MUST invoke ToggleBeginFrame() once per layout pass before
// Clay_BeginLayout.

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

struct ToggleOpts {
	// Layout box size. The bridge blits at the bbox top-left using the
	// sprite's natural pixel dimensions; layout size only governs hit-rect
	// + Clay's box-model placement, not the blit. Pick a size matching the
	// sprite for clean hover bounds.
	Uint16 width  = 16;
	Uint16 height = 16;
	// EffectColor palette tint applied to the sprite before blit. 0 = none.
	// Mirrors the legacy bank-181 path that uses tint 112 for agency icons.
	Uint8 effectColor = 0;
	// Brightness applied via EffectBrightness per selected state. 128 =
	// neutral (no pass). The legacy agency-toggle defaults are 128/32.
	Uint8 selectedBrightness   = 128;
	Uint8 unselectedBrightness = 128;
};

using ToggleClickFn = void (*)(void * user);

struct ToggleHandle {
	bool *         hoveredOut;  // Optional. Written each frame if non-null.
	ToggleClickFn  onClick;     // Optional. Routed through UiAutomationRegistry.
	void *         user;        // Forwarded to onClick.
};

// Resets the per-frame click-adapter + payload arenas. Call once before
// each Clay_BeginLayout.
void ToggleBeginFrame();

// Emits one Toggle subtree. Must be called inside an open CLAY parent
// element scope (or at the layout root). `id` is the Clay element's
// stable ID basis (must be unique within the current layout pass).
void Toggle(Clay_String id,
            Uint8 spriteBank,
            Uint16 spriteIndex,
            bool selected,
            ToggleOpts opts = {},
            ToggleHandle handle = {});

}  // namespace silencer::ui::primitives

#endif
