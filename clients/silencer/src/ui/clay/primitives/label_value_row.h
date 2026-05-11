#ifndef SILENCER_UI_CLAY_PRIMITIVES_LABEL_VALUE_ROW_H
#define SILENCER_UI_CLAY_PRIMITIVES_LABEL_VALUE_ROW_H

// Screen-agnostic Clay primitive for a single form row (label + value).
//
// Emits one LEFT_TO_RIGHT CLAY block sized to (opts.width × opts.height)
// containing:
//
//   • A fixed-width label column (BankText at `opts.variant`).
//   • A growing value column (BankText at `opts.variant`).
//
// The primitive is screen-agnostic: the caller supplies both strings and
// the visual knobs. Variants are an explicit enum reused from BankText. No
// lobby/world/Config references; no internal state. Allocations for any
// non-default brightness / colorRamp / drawAlpha go through BankText's own
// per-frame arena — the caller MUST already have invoked
// BankTextBeginFrame() before Clay_BeginLayout. LabelValueRow has no arena
// of its own.

#include "clay/clay.h"
#include "shared.h"
#include "primitives/bank_text.h"

namespace silencer::ui::primitives {

struct LabelValueRowOpts {
	Uint16          width            = 156;   // Total row width (px).
	Uint16          height           = 14;    // Total row height (px). Matches legacy 14-px label rhythm.
	Uint16          labelWidth       = 72;    // Label column width (px). Value column grows to fill remainder.
	BankTextVariant variant          = BankTextVariant::Body;
	Uint8           labelEffectColor = 0;     // 0 = no tint.
	Uint8           labelBrightness  = 128;
	Uint8           valueEffectColor = 0;
	Uint8           valueBrightness  = 128;
};

// Emits one LabelValueRow subtree. Must be called inside an open CLAY
// parent element scope. `id` is the row's stable Clay ID; both `label` and
// `value` must remain valid until Clay_EndLayout.
void LabelValueRow(Clay_String id,
                   Clay_String label,
                   Clay_String value,
                   LabelValueRowOpts opts = {});

}  // namespace silencer::ui::primitives

#endif
