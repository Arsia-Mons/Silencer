#ifndef SILENCER_UI_CLAY_PRIMITIVES_BANK_BUTTON_H
#define SILENCER_UI_CLAY_PRIMITIVES_BANK_BUTTON_H

// Screen-agnostic Clay primitive for paletted bank-font buttons.
//
// Three variants mirror the legacy Button widget's three lobby usages:
//
//   Variant   Chrome              Text bank/width   Hit rect    Legacy enum
//   Chrome    bank 7 idx 24       134 / 8           156 x 21    B156x21
//   Inline    (none, text-only)   133 / 7           text bbox   BNONE
//   Checkbox  bank 7 idx 19/18    (none)            13 x 13     BCHECKBOX
//
// The primitive owns no state and references no lobby/world/Config. State
// (checkbox selected, currently-hovered, callback bookkeeping) is supplied
// by the caller via opts + handle. The caller is responsible for tracking
// "was this hovered last frame?" if it cares; the primitive only reports
// the current frame's hover via the optional `hoveredOut` field.
//
// Memory: each call may allocate up to two small per-frame structs (a click
// adapter and, for Chrome, a chrome payload + a ClayCustomData header) from
// a fixed-capacity bump arena. Callers MUST invoke BankButtonBeginFrame()
// once per layout pass before Clay_BeginLayout to reset the arena.

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

enum class BankButtonVariant : Uint8 {
	Chrome,    // B156x21 sprite (bank 7 idx 24), heading-bank text centered.
	Inline,    // BNONE — text-only; hit rect is the text bbox.
	Checkbox,  // BCHECKBOX 13x13 (bank 7 idx 19 off / idx 18 on).
};

struct BankButtonOpts {
	bool   selected    = false;  // Checkbox only: true → idx 18 (on); false → idx 19 (off).
	Uint8  effectColor = 0;      // Text tint (palette index). 0 = no tint.
	Uint8  textBrightness = 128; // Inline only override. Chrome/Checkbox derive from hover.
};

using BankButtonClickFn = void (*)(void * user);

struct BankButtonHandle {
	bool *              hoveredOut;  // Optional. Written each frame if non-null.
	BankButtonClickFn   onClick;     // Optional. Fires once per PRESSED_THIS_FRAME.
	void *              user;        // Forwarded to onClick.
};

// Resets the per-frame click-adapter + chrome-payload + ClayCustomData
// arenas. Call once before each Clay_BeginLayout.
void BankButtonBeginFrame();

// Emits one BankButton subtree. Must be called inside an open CLAY parent
// element scope (or at the layout root). `label` is both the rendered text
// AND the Clay element's stable ID — labels must be unique within the
// current Clay layout pass (collisions silently misroute hover/click).
void BankButton(Clay_String label,
                BankButtonVariant variant,
                BankButtonOpts opts = {},
                BankButtonHandle handle = {});

}  // namespace silencer::ui::primitives

#endif
