#pragma once

#include "ui/style/theme.h"

namespace client::ui {

// Design tokens (read-only): the resolved product Theme, installed by
// ThemeProvider. Forwards to the vendored ui::use_theme().
inline const ::ui::Theme &use_tokens() { return ::ui::use_theme(); }

} // namespace client::ui
