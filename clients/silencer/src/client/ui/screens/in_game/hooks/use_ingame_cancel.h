#pragma once

#include "client/ui/hooks/use_cancel.h"
#include "client/ui/hooks/use_ingame_chat.h"  // IngameChat
#include "client/ui/hooks/use_navigation.h"   // Navigation
#include "client/ui/hooks/use_tech.h"         // Tech
#include "client/ui/screens/pause_screen.h"   // PauseScreen

#include <memory>

namespace client::ui {

// The in-match cancel chain (ESC / controller back / the `back` op, via the
// UI-layer cancel router). Captured intent closures only — never a Player/World
// flag. Priority: close chat compose, else close the buy/tech station, else push
// the PauseScreen ("Hit Enter To Quit"). All intents queue.
inline void use_ingame_cancel(const IngameChat &chat, const Tech &tech,
                              const Navigation &nav) {
  use_cancel([chat, tech, nav]() {
    if (chat.active) {
      if (chat.cancel)
        chat.cancel();
    } else if (tech.buy_active || tech.tech_active) {
      if (tech.close)
        tech.close();
    } else if (nav.push) {
      nav.push(std::make_unique<PauseScreen>());
    }
  });
}

} // namespace client::ui
