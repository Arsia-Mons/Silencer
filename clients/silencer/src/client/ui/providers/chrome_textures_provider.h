#pragma once

#include "client/ui/hooks/use_chrome.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Installs the baked legacy-sprite chrome ids for `children`.
::ui::UiElement ChromeTexturesProvider(const ChromeTextures &value,
                                       ::ui::UiChildren children,
                                       const char *key = nullptr);

} // namespace client::ui
