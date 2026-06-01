#pragma once

#include "client/ui/hooks/use_settings.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the settings model. Mounted feature-global in the FrameProvider
// chain between Session and KeyMap (doc §5). The composition root assembles the
// `Settings` (read fields from Config + set_*/commit/revert closures over the
// public subsystems) once per tick and hands it in. The provider holds no game
// handle — only the resolved model.
struct SettingsProviderValue {
  Settings settings = {};
};

::ui::UiElement SettingsProvider(const SettingsProviderValue &value,
                                 ::ui::UiChildren children,
                                 const char *key = nullptr);

} // namespace client::ui
