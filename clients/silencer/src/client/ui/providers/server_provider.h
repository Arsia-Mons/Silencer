#pragma once

#include "ui/runtime/element.h"

class Game;

namespace silencer::game_ui {

// Publishes the live Game handle to the component tree. Mounted outermost in
// the global FrameProvider chain (doc §5: Theme ▸ Server ▸ App ▸ …). Carries the
// handle only — no renderer/mutation-sink/screen-stack plumbing.
struct ServerProviderValue {
  Game *game = nullptr;
};

::ui::UiElement ServerProvider(const ServerProviderValue &value,
                               ::ui::UiChildren children,
                               const char *key = nullptr);

} // namespace silencer::game_ui
