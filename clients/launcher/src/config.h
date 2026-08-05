#ifndef LAUNCHER_CONFIG_H
#define LAUNCHER_CONFIG_H

#include <string>

namespace launcher {

// The launcher's on-disk config, in the per-OS app-data dir (see dir_path()).
// Installs are per-channel: each channel extracts into {base_dir}/{channel}
// and tracks its own installed version.
struct Config {
  std::string channel = "stable"; // active channel: "stable" | "nightly"
  std::string base_dir;           // install root; channel dirs live inside
  std::string installed_stable;   // "" = channel not installed
  std::string installed_nightly;
  std::string manifest_url_stable;
  std::string manifest_url_nightly;
  std::string announcements_url;
  std::string releases_url; // GitHub Releases API for the RELEASES tab
  std::string lobby_host;   // ping chip + sign-in target
  int lobby_port = 517;

  // The per-OS config dir and the launcher.json path inside it:
  // macOS   ~/Library/Application Support/Silencer Launcher
  // Windows %APPDATA%\Silencer Launcher
  // Linux   ~/.config/silencer-launcher
  static std::string dir_path();
  static std::string file_path();

  // Fill any empty field with its default.
  void apply_defaults();

  // Read the config file; on a missing/garbage file, populate defaults and
  // write a fresh file. Always leaves *this in a usable state.
  bool load();

  // Atomically (temp + rename) write the config, creating the dir if needed.
  bool save() const;

  // {base_dir}/{channel} and the game executable inside it.
  std::string channel_dir(const std::string &channel) const;
  std::string game_binary(const std::string &channel) const;

  const std::string &installed(const std::string &channel) const;
  void set_installed(const std::string &channel, const std::string &version);
};

// Where the *game* keeps its own per-user files (config.cfg, keybinds/,
// level/download/, replays/). Shown read-only by the SETTINGS tab.
//
// This mirrors GetDataDir() in clients/silencer/src/platform/os.cpp by hand
// rather than compiling that file in: os.cpp includes the game's shared.h
// (far outside the launcher's dep closure), and GetDataDir() *creates* the
// directory as a side effect — opening a settings page must not conjure a
// game data dir for someone who has never run the game. Keep the two in sync.
std::string game_data_dir();

} // namespace launcher

#endif
