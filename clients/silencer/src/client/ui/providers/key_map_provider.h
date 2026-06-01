#pragma once

#include "client/ui/hooks/use_key_map.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the key-map model. Mounted feature-global in the FrameProvider
// chain between SettingsProvider and UpdaterProvider (doc §5). The composition
// root resolves the rows-of-combos view + the six mutation closures (over the
// public Game/keybinds seam) once per tick and hands it in. The provider holds
// no game/keymap handle — only the resolved model.
struct KeyMapProviderValue {
  KeyMap key_map = {};
};

::ui::UiElement KeyMapProvider(const KeyMapProviderValue &value,
                               ::ui::UiChildren children,
                               const char *key = nullptr);

} // namespace client::ui
