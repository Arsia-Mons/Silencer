#ifndef SILENCER_UI_CLAY_PRIMITIVES_BANK_TEXT_H
#define SILENCER_UI_CLAY_PRIMITIVES_BANK_TEXT_H

// Screen-agnostic Clay primitive for paletted bank-font text.
//
// Emits a single CLAY_TEXT element configured for the Silencer text-bank
// renderer (Renderer::DrawText). The primitive owns no state and references
// no lobby/world/Config; the caller supplies the string, variant, and any
// optional effect knobs.
//
// Variants encode (bank, cell width) presets used across the client UI:
//
//   Variant   Bank   Cell width   Cell height
//   Title     135    11           19
//   Heading   134    8            15
//   Body      133    6            11
//   BodySm    133    7            11
//
// `effectColor` is the palette index for the tint (Overlay::effectcolor in
// the legacy widget system). `brightness == 128` is the neutral default
// (matches Overlay::effectbrightness's default). `colorRamp` and `drawAlpha`
// match Overlay::textcolorramp and Overlay::drawalpha respectively.
//
// Memory: when any non-default brightness / colorRamp / drawAlpha is used,
// the primitive allocates a BankTextDrawData from a per-frame bump arena.
// Callers MUST invoke BankTextBeginFrame() once per layout pass before
// Clay_BeginLayout to reset the arena. The arena holds stable pointers for
// the duration of the layout; allocations beyond `kBankTextUserDataCapacity`
// fall back to the default render path (logged silently — bump capacity if
// you see this).

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

enum class BankTextVariant : Uint8 {
	Title,    // bank 135, cell width 11
	Heading,  // bank 134, cell width 8
	Body,     // bank 133, cell width 6
	BodySm,   // bank 133, cell width 7
};

struct BankTextOpts {
	Uint8 effectColor = 0;    // palette index; 0 = no tint (renderer default).
	Uint8 brightness  = 128;  // 128 = neutral (legacy Overlay default).
	bool  colorRamp   = false;
	bool  drawAlpha   = false;
};

// Resets the per-frame BankTextDrawData arena. Call once before each
// Clay_BeginLayout if any BankText call in this frame uses non-default
// brightness / colorRamp / drawAlpha. Safe to call unconditionally.
void BankTextBeginFrame();

// Emits one CLAY_TEXT element. Must be called inside an open CLAY element
// scope. `text` must remain valid until Clay_EndLayout (Clay does not copy
// the string contents).
void BankText(Clay_String text,
              BankTextVariant variant,
              BankTextOpts opts = BankTextOpts{});

}  // namespace silencer::ui::primitives

#endif
