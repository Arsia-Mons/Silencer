#ifndef LAUNCHER_UI_H
#define LAUNCHER_UI_H

#include "app.h"

#include "ui/runtime/element.h"
#include "ui/runtime/react.h"
#include "ui/style/theme.h"

#include <functional>
#include <memory>
#include <string>

namespace client::ui {
class UiScreen;
}

namespace launcher {

// Named intent closures the UI calls; bound once by main() to the App
// (open_url is bound to SDL_OpenURL — the view stays SDL-free).
struct Intents {
  std::function<void(const std::string &)> set_channel;
  std::function<void()> refresh;
  std::function<void(const std::string &)> install;
  std::function<void(const std::string &)> uninstall;
  std::function<void(const std::string &)> set_base_dir;
  std::function<void()> browse_base_dir; // OS folder picker; result -> set_base_dir
  std::function<void()> play;
  std::function<void(const std::string &, const std::string &)> sign_in;
  std::function<void()> sign_out;
  std::function<void()> self_update; // download + apply the launcher's own update
  std::function<void(const std::string &)> open_url;
};

// What the view reads for one frame: a snapshot pointer + the intents. Delivered
// to the tree via LauncherContext (a normal ReactContext), read by use_launcher.
struct ViewModel {
  const AppSnapshot *snap = nullptr;
  const Intents *intents = nullptr;
  // Backdrop texture for this frame (0 = plain fill). main() owns the bake +
  // the shuffle/cycle; the view just draws whatever id it is handed.
  uint32_t bg_texture = 0;
  // Canonical SILENCER logo (bank 208 frame 60); 0 = fall back to a text
  // wordmark. Native size travels with it so the topbar can hold the aspect.
  uint32_t logo_texture = 0;
  int logo_w = 0, logo_h = 0;
  // Where the running launcher itself lives (SETTINGS tab, read-only). Comes
  // from main() because it needs SDL_GetBasePath and this view stays SDL-free.
  std::string install_dir;
};

extern ReactContext LauncherContext; // current = const ViewModel*

const ViewModel &use_launcher();

// The two halves every view reads. Both fall back to an empty singleton, so a
// component can be rendered outside the provider (tests, a bare frame) without
// dereferencing a null pointer.
const AppSnapshot &use_snapshot();
const Intents &use_intents();

const ::ui::Theme &launcher_theme();

// Wraps the screen root with the theme + launcher-view-model providers. Passed
// to UiPipeline::set_frame_provider; `vm` must stay valid for the frame.
::ui::UiElement launcher_providers(::ui::UiElement child, const ViewModel *vm);

std::unique_ptr<client::ui::UiScreen> make_launcher_screen();

} // namespace launcher

#endif
