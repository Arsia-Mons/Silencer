#ifndef LAUNCHER_CONFIG_H
#define LAUNCHER_CONFIG_H

#include <string>
#include <vector>

namespace launcher {

struct ServerCfg {
  std::string name;
  std::string host;
  int port = 517;
};

// The launcher's on-disk config. Keys + path are a shared contract with the
// competing Tauri launcher — do not rename keys or move the file.
// Path: ~/.config/silencer-launcher/launcher.json
struct Config {
  std::string channel = "stable"; // "stable" | "nightly"
  std::string installed_version;  // "" = nothing installed yet
  std::string install_dir;        // where updates extract to
  std::string game_binary;        // executable Play launches
  std::vector<ServerCfg> servers;
  int last_server = 0;
  std::string manifest_url_stable;
  std::string manifest_url_nightly;
  std::string announcements_url;

  // ~/.config/silencer-launcher and the launcher.json path inside it.
  static std::string dir_path();
  static std::string file_path();

  // Fill any empty field with its default (also seeds the default server list).
  void apply_defaults();

  // Read the config file; on a missing/garbage file, populate defaults and
  // write a fresh file. Always leaves *this in a usable state.
  bool load();

  // Atomically (temp + rename) write the config, creating the dir if needed.
  bool save() const;

  const ServerCfg *selected_server() const;
};

} // namespace launcher

#endif
