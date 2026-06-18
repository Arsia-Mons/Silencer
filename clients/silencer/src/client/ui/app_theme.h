#pragma once

// app_theme(): the product look (dark slate, accent blue, 8px radius, control
// gradients). The VALUES live here in client/ — src/ui/ keeps only a neutral,
// flat fallback (default_theme()). ThemeProvider installs app_theme() into the
// ThemeContext so use_theme() returns the slate palette for the whole subtree.

#include "ui/style/theme.h"
#include "ui/runtime/element.h"

namespace client::ui {

// The product design system. Function-static: its address is stable, so the
// provider can hand a const_cast pointer straight to the ThemeContext with no
// copy_value indirection.
const ::ui::Theme &app_theme();

// Installs app_theme() into ui::ThemeContext for `children` (mirrors
// ServerProvider). Place OUTERMOST so the whole tree sees the slate palette.
::ui::UiElement ThemeProvider(::ui::UiChildren children);

} // namespace client::ui
