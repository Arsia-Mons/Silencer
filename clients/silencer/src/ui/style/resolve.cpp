#include "resolve.h"

namespace ui {

VisualStyle resolve(const RoleStyle &role, const StyleStatePatch &ov,
                    const InteractionState &st) {
  VisualStyle vs = role.base; // start dense
  apply(vs, ov.base);         // override base on top of role base
  if (st.hovered) {
    apply(vs, role.hover);
    apply(vs, ov.hover);
  }
  if (st.focus_visible) {
    apply(vs, role.focus_visible);
    apply(vs, ov.focus_visible);
  }
  if (st.pressed) {
    apply(vs, role.pressed);
    apply(vs, ov.pressed);
  }
  if (st.checked) {
    apply(vs, role.checked);
    apply(vs, ov.checked);
  }
  if (st.active) {
    apply(vs, role.active);
    apply(vs, ov.active);
  }
  if (st.disabled) {
    apply(vs, role.disabled); // disabled wins
    apply(vs, ov.disabled);
  }
  return vs;
}

} // namespace ui
