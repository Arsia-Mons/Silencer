#ifndef SILENCER_UI_CLAY_PRIMITIVES_SCROLL_LIST_H
#define SILENCER_UI_CLAY_PRIMITIVES_SCROLL_LIST_H

// Screen-agnostic Clay primitive for a vertical list of text rows with a
// sprite-rendered scrollbar on the right. Replaces the legacy SelectBox +
// ScrollBar pair.
//
// Inputs are immutable. Selection routes through UiInteractionRegistry as a
// typed UiAction with the row index. The caller owns `selectedIndex` and
// `scrollPosition`; the primitive only reads them.
//
// Memory: each call allocates one ScrollBar payload + one ClayCustomData
// header from fixed-capacity bump arenas. Callers MUST invoke
// ScrollListBeginFrame() once per layout pass before Clay_BeginLayout.

#include "clay/clay.h"
#include "primitives/text.h"
#include "shared.h"

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::ui::primitives {

struct ScrollListOpts {
	// Layout box.
	Uint16 width      = 200;
	Uint16 height     = 130;
	Uint8  lineHeight = 13;  // legacy SelectBox::lineheight default.

	// Selection highlight palette index for the row's full-width bar.
	Uint8  highlightColor = 180;  // generic

	TextOpts text = TextOpts{};

	// Scrollbar sprite + sizing. The primitive has no opinion on which sprite
	// bank carries the scrollbar art — the caller must spell it.
	Uint8  scrollbarBank;
	Uint16 scrollbarTrackIndex  = 9;
	Uint16 scrollbarThumbIndex  = 10;
	Uint16 scrollbarWidth       = 8;
	Uint16 scrollbarGap         = 0;  // px between rows column and scrollbar.
};

struct ScrollListHandle {
	bool *      hoveredOut;    // Optional. Written each frame if non-null.
	const char * actionId;     // Optional row action ID prefix registered for input routing.
	UiInteractionRegistry * interactions;  // Required when actionId is set.
	int * hoveredIndexOut = nullptr;  // Optional. Hovered absolute row index.
	bool requestSelectedInitialFocus = false;
	bool requestSelectedFocus = false;
};

// Resets the per-frame click-adapter + payload arenas. Call once before
// each Clay_BeginLayout.
void ScrollListBeginFrame();

// Emits one ScrollList subtree. Must be called inside an open CLAY parent
// element scope (or at the layout root). `id` is the Clay element's stable
// ID basis (must be unique within the current layout pass — the primitive
// derives row IDs from it).
//
// `items` must point to `itemCount` Clay_String values that remain valid
// until Clay_EndLayout (Clay does not copy the string contents).
//
// `selectedIndex` of -1 means "no selection" (no highlight bar drawn).
// `scrollPosition` is in row units: row `scrollPosition` is the first
// visible row at the top of the rendered area.
void ScrollList(Clay_String id,
                const Clay_String * items,
                int                 itemCount,
                int                 selectedIndex,
                Uint16              scrollPosition,
                ScrollListOpts      opts = {},
                ScrollListHandle    handle = {});

}  // namespace silencer::ui::primitives

#endif
