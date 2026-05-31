#pragma once

// Theme: a single immutable POD owned by the app, delivered to a subtree via a
// normal ReactContext. Components read it with use_theme(), resolve their own
// VisualStyle at authoring time, and pass it as a prop. The renderer never sees
// the theme; it is not queryable during the draw DFS (context unwinds before
// build_draw_list). See design §4, §6.

#include "../runtime/react.h" // ReactContext, use_context
#include "style_patch.h"
#include "visual_style.h"

namespace ui {

// A widget role's dense default plus one sparse StylePatch per interaction state.
// resolve() (resolve.h) layers active patches over base, low->high.
struct RoleStyle {
  VisualStyle base{};
  StylePatch hover{};
  StylePatch focus_visible{};
  StylePatch pressed{};
  StylePatch checked{};
  StylePatch active{};
  StylePatch disabled{};
};

struct Theme {
  RoleStyle box{};
  RoleStyle text{};
  RoleStyle button{};
  RoleStyle input{};
  RoleStyle checkbox{};
  RoleStyle checkbox_mark{};
  RoleStyle dialog{};
  // Global tokens (loose colors a component reaches for when composing a subpart
  // that has no RoleStyle):
  Color focus_ring{};
  Color text_default{};
  Color text_disabled{};
  Color caret{};
  Color selection{};
};

// One context carries the theme (current = const Theme*). Header-inline so
// there is exactly one definition.
inline ReactContext ThemeContext = {};

const Theme &default_theme(); // defined in default_theme.cpp

inline const Theme &use_theme() {
  const Theme *t = static_cast<const Theme *>(use_context(&ThemeContext));
  return t ? *t : default_theme();
}

} // namespace ui
