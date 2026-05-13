#ifndef SILENCER_UI_CLAY_PRIMITIVES_SCROLL_TEXT_BOX_H
#define SILENCER_UI_CLAY_PRIMITIVES_SCROLL_TEXT_BOX_H

// Screen-agnostic Clay primitive for a multi-line text box with per-line
// color/brightness and optional bottom-anchored fill. Replaces the legacy
// the old retained TextBox widget.
//
// The primitive is stateless: the caller owns the line vector and the scroll
// position. Auto-scroll-on-append is screen policy; see
// `ScrollTextBoxAutoScroll` for the canonical computation.
//
// Memory: each call allocates one optional ScrollBar payload + one
// ClayCustomData header from fixed-capacity bump arenas. Callers MUST invoke
// ScrollTextBoxBeginFrame() once per layout pass before Clay_BeginLayout.

#include "clay/clay.h"
#include "primitives/bank_text.h"
#include "shared.h"

namespace silencer::ui::primitives {

struct ScrollTextBoxLine {
	Clay_String text;
	Uint8       effectColor;  // palette idx; 0 = neutral.
	Uint8       brightness;   // 128 = neutral (legacy default).
	Uint16      indent;       // px x-offset within the row (0 = flush left).
};

// `BottomUp` anchors the visible block to the bottom edge of the box so that
// when the line vector is short of filling the box, blank space sits at the
// top. Mirrors the legacy `bottomtotop` flag.
enum class ScrollTextBoxOrigin : Uint8 {
	TopDown,
	BottomUp,
};

struct ScrollTextBoxOpts {
	Uint16 width      = 200;
	Uint16 height     = 100;
	Uint8  lineHeight = 11;  // legacy TextBox::lineheight default.

	BankTextVariant     textVariant = BankTextVariant::Body;  // bank 133, cell w=6.
	ScrollTextBoxOrigin origin      = ScrollTextBoxOrigin::TopDown;

	// Optional sprite-rendered scrollbar on the right. When `showScrollbar`
	// is false, no scrollbar element is emitted and the rows column fills
	// the full width. `scrollbarBank` is the palette/sprite bank that holds
	// the track + thumb cells — caller-supplied because the primitive owns
	// no opinion about lobby vs. world vs. demo chrome sources.
	bool   showScrollbar       = false;
	Uint8  scrollbarBank;
	Uint16 scrollbarTrackIndex = 9;
	Uint16 scrollbarThumbIndex = 10;
	Uint16 scrollbarWidth      = 8;
	Uint16 scrollbarGap        = 0;
};

struct ScrollTextBoxHandle {
	bool * hoveredOut = nullptr;  // Written each frame if non-null.
};

// Resets the per-frame scrollbar-payload + custom-data arenas. Call once
// before each Clay_BeginLayout.
void ScrollTextBoxBeginFrame();

// Emits one ScrollTextBox subtree. `id` is the Clay element's stable ID
// basis (must be unique within the layout pass — the primitive derives row
// IDs from it).
//
// `scrollPosition` is in row units: row `scrollPosition` is the first
// visible row at the top of the rendered area. Out-of-window rows are not
// emitted (no Clay scissor needed for top/bottom culling).
//
// `lines` must point to `lineCount` ScrollTextBoxLine values whose
// `text.chars` storage remains valid until Clay_EndLayout (Clay does not
// copy text content).
void ScrollTextBox(Clay_String                 id,
                   const ScrollTextBoxLine *   lines,
                   int                         lineCount,
                   Uint16                      scrollPosition,
                   ScrollTextBoxOpts           opts = {},
                   ScrollTextBoxHandle         handle = {});

// Screen helper. Returns the scroll position that keeps the bottom-most
// content visible after a line append, IF the caller was already pinned to
// the bottom before the append. Otherwise returns `prevScrollPosition`
// unchanged.
//
// Mirrors the legacy `TextBox::AddLine(scroll=true)` behavior, but as a
// pure function over caller-owned state rather than a side effect inside
// the widget.
Uint16 ScrollTextBoxAutoScroll(Uint16 prevScrollPosition,
                               int    prevLineCount,
                               int    newLineCount,
                               Uint8  lineHeight,
                               Uint16 height);

}  // namespace silencer::ui::primitives

#endif
