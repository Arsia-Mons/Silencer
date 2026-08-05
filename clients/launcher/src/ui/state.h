#ifndef LAUNCHER_UI_STATE_H
#define LAUNCHER_UI_STATE_H

// Per-session UI state. One struct, owned by RootView's fiber (use_state) and
// handed down to the views that read or write it. It holds only what the user
// does to the launcher — everything the launcher knows about the world comes
// from the AppSnapshot in LauncherContext.

#include <cstdlib>
#include <string>

namespace launcher {

struct UiState {
  int tab = 0; // 0 = news, 1 = releases, 2 = settings
  int sel_news = 0;
  int sel_release = 0;
  bool drop_open = false;
  bool auth_open = false;
  bool editing_loc = false;
  std::string loc_text;
  std::string user_text;
  std::string pass_text;
  std::string confirm_uninstall; // channel awaiting confirmation, "" = none
  bool font_qa = false;          // show the font QA panel instead of content
};

// Offscreen self-verify (SILENCER_LAUNCHER_SHOT) can't inject clicks, so
// SILENCER_LAUNCHER_UI_STATE seeds the initial UI state to screenshot the
// overlays: comma-separated tokens "releases", "settings", "drop", "auth",
// "confirm", "fontqa".
inline UiState initial_ui_state() {
  UiState st;
  const char *env = getenv("SILENCER_LAUNCHER_UI_STATE");
  if (!env || !env[0])
    return st;
  std::string s = env;
  if (s.find("releases") != std::string::npos)
    st.tab = 1;
  if (s.find("settings") != std::string::npos)
    st.tab = 2;
  if (s.find("drop") != std::string::npos)
    st.drop_open = true;
  if (s.find("auth") != std::string::npos)
    st.auth_open = true;
  if (s.find("confirm") != std::string::npos)
    st.confirm_uninstall = "stable";
  if (s.find("fontqa") != std::string::npos)
    st.font_qa = true;
  return st;
}

} // namespace launcher

#endif
