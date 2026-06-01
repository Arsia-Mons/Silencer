#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace client::ui {

// Installs the app-global capability callbacks for `children`. The composition
// root supplies `quit` (a closure over the live Game); the provider holds only
// the callback, never the handle.
struct AppProviderValue {
  std::function<void()> quit = {};
};

::ui::UiElement AppProvider(const AppProviderValue &value,
                            ::ui::UiChildren children, const char *key = nullptr);

} // namespace client::ui
