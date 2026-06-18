#pragma once

#include "ui/style/theme.h"

namespace client::ui {

// Design tokens (read-only): the resolved product Theme for the current
// subtree. Backed by ui::ThemeContext, installed OUTERMOST by ThemeProvider
// (app_theme.h). Falls back to ui::default_theme() when no provider is mounted.
// Doc §6: use_tokens() — read-only. Enriching the VALUES inside app_theme()
// leaves the hook surface stable. Header-inline forward to the
// vendored golden ui::use_theme() (kept verbatim, hence the wrapper).
inline const ::ui::Theme &use_tokens() { return ::ui::use_theme(); }

} // namespace client::ui
