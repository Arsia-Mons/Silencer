#pragma once

// resolve(): the single function that turns a RoleStyle + a component's
// per-instance StyleStatePatch override + the component's current
// InteractionState into one dense VisualStyle, run INSIDE the component at
// authoring time. For each state slot the role patch is layered first, then the
// override patch on top (override wins per-field). State order (low->high):
// base -> hover -> focus_visible -> pressed -> checked -> active -> disabled.
// See design §5. A bare StylePatch (or {}) implicitly becomes a base-only
// StyleStatePatch.

#include "interaction.h"
#include "style_patch.h"
#include "theme.h"
#include "visual_style.h"

namespace ui {

VisualStyle resolve(const RoleStyle &role, const StyleStatePatch &ov,
                    const InteractionState &st);

} // namespace ui
