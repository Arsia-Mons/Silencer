#pragma once

#include "client/ui/hooks/use_session.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the session phase to the component tree. Mounted feature-global in
// the FrameProvider chain; the composition root computes `phase` once per tick
// from the game's state machine and hands it in. The provider holds no game
// handle — only the resolved projection (doc §5).
struct SessionProviderValue {
  SessionPhase phase = SessionPhase::MainMenu;
};

::ui::UiElement SessionProvider(const SessionProviderValue &value,
                                ::ui::UiChildren children,
                                const char *key = nullptr);

} // namespace client::ui
