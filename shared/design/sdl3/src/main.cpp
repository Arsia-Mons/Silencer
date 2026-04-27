#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "dump_runner.h"
#include "screens.h"

using namespace silencer;

namespace {

// Registry of every screen the hydration tool can dump. Order is
// insignificant; the lookup is by `name`. The first entry doubles as the
// default when SILENCER_DUMP_SCREEN is unset (preserving today's
// "main_menu by default" behaviour).
const std::vector<ScreenSpec> &Registry() {
  static const std::vector<ScreenSpec> kScreens = {
      {"main_menu",         {6, 133, 135, 208},   1, 0, &ComposeMainMenu},
      {"options",           {6, 135},             1, 0, &ComposeOptions},
      {"options_audio",     {6, 135},             1, 0, &ComposeOptionsAudio},
      {"options_display",   {6, 135},             1, 0, &ComposeOptionsDisplay},
      {"options_controls",  {6, 7, 134, 135},     1, 0, &ComposeOptionsControls},
      {"lobby_connect",     {7, 133, 134, 135},   2, 0, &ComposeLobbyConnect},
      {"lobby",             {7, 133, 134, 135, 181}, 2, 0, &ComposeLobby},
      {"lobby_gamecreate",  {7, 133, 134, 135, 181}, 2, 0, &ComposeLobbyGameCreate},
      {"lobby_gamejoin",    {7, 133, 134, 135, 181}, 2, 0, &ComposeLobbyGameJoin},
      {"lobby_gametech",    {7, 133, 134, 135, 181}, 2, 0, &ComposeLobbyGameTech},
      {"lobby_gamesummary", {7, 133, 134, 135},   0, 0, &ComposeLobbyGameSummary},
      {"updating",          {7, 133, 134},        1, 0, &ComposeUpdating},
  };
  return kScreens;
}

}  // namespace

int main(int argc, char **argv) {
  std::string assets_dir;
  if (argc >= 2) {
    assets_dir = argv[1];
  } else {
    const char *base = SDL_GetBasePath();
    assets_dir = (base ? std::string(base) + "../../../assets"
                       : std::string("../../../assets"));
  }

  const char *dump = std::getenv("SILENCER_DUMP_DIR");
  if (dump && *dump) {
    const char *screen = std::getenv("SILENCER_DUMP_SCREEN");
    std::string screen_str = (screen && *screen) ? screen : "main_menu";

    const auto &registry = Registry();
    for (const auto &spec : registry) {
      if (screen_str == spec.name) {
        return RunScreenDump(spec, assets_dir, dump);
      }
    }
    // Unknown screen name: fall back to the default (first registered).
    return RunScreenDump(registry.front(), assets_dir, dump);
  }

  // Interactive mode: open a window and exit. The meaningful path is dump
  // mode; this branch keeps the binary minimally useful from the desktop.
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window *win =
      SDL_CreateWindow("Silencer Design", 640, 480, SDL_WINDOW_RESIZABLE);
  if (!win) {
    std::fprintf(stderr, "no window: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
