#ifndef SILENCER_UI_CLAY_PRIMITIVES_FORM_BORDER_H
#define SILENCER_UI_CLAY_PRIMITIVES_FORM_BORDER_H

// Screen-agnostic Clay primitive for a 1-px palette-indexed border.
//
// `FormBorder(paletteColor)` returns a Clay_BorderElementConfig with a 1-px
// stroke on all four sides at the caller-supplied palette index. Drop into
// any CLAY block's `.border = FormBorder(idx)` to stroke that element's
// bounding box.
//
// The primitive owns no state and references no lobby/world/Config. It is a
// configuration helper, not a stateful subtree emitter — there is no
// FormBorderBeginFrame() because no per-frame storage is needed.
//
// Color is the only knob. Width is fixed at 1 px on all sides; for thicker
// strokes, use multiple stacked FormBorders or a different primitive.

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

inline ::Clay_BorderElementConfig FormBorder(Uint8 paletteColor) {
	return ::Clay_BorderElementConfig{
		.color = { static_cast<float>(paletteColor), 0.0f, 0.0f, 255.0f },
		.width = { /*left=*/1, /*right=*/1, /*top=*/1, /*bottom=*/1,
		           /*betweenChildren=*/0 },
	};
}

}  // namespace silencer::ui::primitives

#endif
