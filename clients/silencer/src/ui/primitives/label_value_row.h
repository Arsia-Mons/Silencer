#ifndef SILENCER_UI_CLAY_PRIMITIVES_LABEL_VALUE_ROW_H
#define SILENCER_UI_CLAY_PRIMITIVES_LABEL_VALUE_ROW_H

// Screen-agnostic Clay primitive for a single form row (label + value).
//
// Emits one LEFT_TO_RIGHT CLAY block sized to (opts.width × opts.height)
// containing:
//
//   • A fixed-width label column.
//   • A growing value column.
//
// The primitive is screen-agnostic: the caller supplies both strings and
// text style. No lobby/world/Config references; no internal state.

#include "clay/clay.h"
#include "shared.h"
#include "primitives/text.h"

namespace silencer::ui::primitives {

struct LabelValueRowOpts {
	Uint16          width;                    // Total row width (px). Required — primitive carries no opinion on form size.
	Uint16          height           = 14;    // Total row height (px). Sized for a single text cell.
	Uint16          labelWidth;               // Label column width (px). Required — caller sizes for its longest label. Value column fills (width - labelWidth).
	TextOpts        text = TextOpts{};
	TextEffect      labelEffect = TextEffect::Default();
	TextEffect      valueEffect = TextEffect::Default();
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
