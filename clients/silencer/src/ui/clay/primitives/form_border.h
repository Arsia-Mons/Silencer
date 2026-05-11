#ifndef SILENCER_UI_CLAY_PRIMITIVES_FORM_BORDER_H
#define SILENCER_UI_CLAY_PRIMITIVES_FORM_BORDER_H

// Screen-agnostic Clay primitive for a 1-px palette-indexed form stroke.
//
// `FormBorder()` returns a Clay_BorderElementConfig configured for a 1-px
// border on all four sides at palette index 220 (the lobby's bright-green
// chrome stroke — sampled directly from sprite (7, 8) in the legacy form
// outline path). Drop into any CLAY block's `.border = FormBorder()` to
// stroke that element's bounding box.
//
// The primitive owns no state and references no lobby/world/Config. It is a
// configuration helper, not a stateful subtree emitter — there is no
// FormBorderBeginFrame() because no per-frame storage is needed.
//
// Color is the only knob. Width is fixed to 1 px on all sides to match the
// legacy form chrome's strict 1-px decorative edge.

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

inline ::Clay_BorderElementConfig FormBorder(Uint8 paletteColor = 220) {
	return ::Clay_BorderElementConfig{
		.color = { static_cast<float>(paletteColor), 0.0f, 0.0f, 255.0f },
		.width = { /*left=*/1, /*right=*/1, /*top=*/1, /*bottom=*/1,
		           /*betweenChildren=*/0 },
	};
}

}  // namespace silencer::ui::primitives

#endif
