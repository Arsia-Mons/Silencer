#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace client::ui {

// Installs the app-global capability callbacks for `children`. The composition
// root supplies `quit` (a closure over the live Game); the provider holds only
// the callback, never the handle.
struct AppProviderValue {
  std::function<void()> quit = {};
  // Build version string (a static string literal owned by the composition root,
  // safe to copy by pointer); shown in the main-menu footer.
  const char *version = "";
  // Logical UI canvas width (see App::canvas_w).
  float canvas_w = 0.0f;
};

::ui::UiElement AppProvider(const AppProviderValue &value,
                            ::ui::UiChildren children, const char *key = nullptr);

} // namespace client::ui
