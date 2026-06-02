#pragma once

#include <functional>

namespace client::ui {

// App-global capabilities that aren't tied to gameplay state: today just the
// quit affordance. `quit()` routes to the public `Game::quitRequested` bool via
// a closure the composition root installs (doc §6). App-shell, not game-coupled
// (it never names Game), so it lives in client::ui.
struct App {
  bool can_quit = false;
  std::function<void()> quit = {};
  // Build version string for the menu footer (e.g. "v00058"); "" if unset.
  const char *version = "";
};

App use_app();

} // namespace client::ui
