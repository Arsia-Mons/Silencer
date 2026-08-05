#include "config.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define launcher_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define launcher_mkdir(p) mkdir(p, 0755)
#endif

namespace launcher {

using nlohmann::json;

namespace {

std::string home_dir() {
  const char *h = getenv("HOME");
  if (h && h[0])
    return h;
#ifdef _WIN32
  h = getenv("USERPROFILE");
  if (h && h[0])
    return h;
#endif
  return ".";
}

#ifdef _WIN32
// %APPDATA% / %LOCALAPPDATA%, normalized to forward slashes: mkdir_p only
// walks '/' to create intermediates, and os.cpp normalizes the same way.
std::string win_dir(const char *var, const char *fallback_sub) {
  const char *v = getenv(var);
  std::string d = (v && v[0]) ? std::string(v) : home_dir() + fallback_sub;
  for (char &c : d)
    if (c == '\\')
      c = '/';
  return d;
}
#endif

void mkdir_p(const std::string &path) {
  std::string cur;
  for (size_t i = 0; i < path.size(); ++i) {
    cur += path[i];
    if (path[i] == '/' && cur.size() > 1)
      launcher_mkdir(cur.c_str());
  }
  launcher_mkdir(path.c_str());
}

} // namespace

std::string Config::dir_path() {
  // SILENCER_LAUNCHER_CONFIG overrides the file path (for testing); the config
  // dir is then its parent.
  const char *env = getenv("SILENCER_LAUNCHER_CONFIG");
  if (env && env[0]) {
    std::string p = env;
    size_t slash = p.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
  }
  // Per-OS, mirroring GetDataDir() in clients/silencer/src/platform/os.cpp.
  // The launcher gets its own folder beside the client's "Silencer" rather
  // than writing into a directory the game owns.
#ifdef _WIN32
  return win_dir("APPDATA", "/AppData/Roaming") + "/Silencer Launcher";
#elif defined(__APPLE__)
  return home_dir() + "/Library/Application Support/Silencer Launcher";
#else
  return home_dir() + "/.config/silencer-launcher";
#endif
}

std::string Config::file_path() {
  const char *env = getenv("SILENCER_LAUNCHER_CONFIG");
  if (env && env[0])
    return env;
  return dir_path() + "/launcher.json";
}

void Config::apply_defaults() {
  if (channel != "nightly")
    channel = "stable";
  // Games go where each OS puts per-user applications, not into the
  // launcher's config dir — on macOS the installed Silencer.app then sits in
  // ~/Applications, where a user expects to find and launch it.
  if (base_dir.empty())
#ifdef _WIN32
    base_dir = win_dir("LOCALAPPDATA", "/AppData/Local") + "/Programs/Silencer";
#elif defined(__APPLE__)
    base_dir = home_dir() + "/Applications/Silencer";
#else
    base_dir = home_dir() + "/.local/share/silencer-launcher";
#endif
  if (manifest_url_stable.empty())
    manifest_url_stable =
        "https://github.com/Arsia-Mons/Silencer/releases/latest/download/update.json";
  if (manifest_url_nightly.empty())
    manifest_url_nightly =
        "https://github.com/Arsia-Mons/Silencer/releases/download/latest/update.json";
  if (manifest_url_launcher.empty())
    manifest_url_launcher =
        "https://github.com/Arsia-Mons/Silencer/releases/latest/download/update-launcher.json";
  if (announcements_url.empty())
    announcements_url = "https://arsiamons.com/announcements.json";
  if (releases_url.empty())
    releases_url = "https://api.github.com/repos/Arsia-Mons/Silencer/releases?per_page=20";
  if (lobby_host.empty())
    lobby_host = "lobby.arsiamons.com";
  if (lobby_port <= 0 || lobby_port > 65535)
    lobby_port = 517;
}

bool Config::load() {
  std::ifstream in(file_path());
  if (in) {
    try {
      json j;
      in >> j;
      // Type-guarded field reads: one mismatched field falls back to its
      // default instead of aborting the whole parse.
      auto str = [&](const char *k, std::string &dst) {
        if (j.contains(k) && j[k].is_string())
          dst = j[k].get<std::string>();
      };
      str("channel", channel);
      str("base_dir", base_dir);
      str("installed_stable", installed_stable);
      str("installed_nightly", installed_nightly);
      str("manifest_url_stable", manifest_url_stable);
      str("manifest_url_nightly", manifest_url_nightly);
      str("manifest_url_launcher", manifest_url_launcher);
      str("announcements_url", announcements_url);
      str("releases_url", releases_url);
      str("lobby_host", lobby_host);
      if (j.contains("lobby_port") && j["lobby_port"].is_number_integer())
        lobby_port = j["lobby_port"].get<int>();
    } catch (const std::exception &e) {
      fprintf(stderr, "[launcher] config parse error (%s); using defaults\n", e.what());
    }
  }
  apply_defaults();
  // Persist so a first run (or a partial file) leaves a complete config on disk.
  return save();
}

bool Config::save() const {
  json j;
  j["channel"] = channel;
  j["base_dir"] = base_dir;
  j["installed_stable"] = installed_stable;
  j["installed_nightly"] = installed_nightly;
  j["manifest_url_stable"] = manifest_url_stable;
  j["manifest_url_nightly"] = manifest_url_nightly;
  j["manifest_url_launcher"] = manifest_url_launcher;
  j["announcements_url"] = announcements_url;
  j["releases_url"] = releases_url;
  j["lobby_host"] = lobby_host;
  j["lobby_port"] = lobby_port;

  mkdir_p(dir_path());
  const std::string path = file_path();
  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      fprintf(stderr, "[launcher] cannot write %s\n", tmp.c_str());
      return false;
    }
    out << j.dump(2) << "\n";
  }
#ifdef _WIN32
  // C rename() refuses to replace an existing file on Windows.
  if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
#else
  if (rename(tmp.c_str(), path.c_str()) != 0) {
#endif
    fprintf(stderr, "[launcher] cannot rename %s -> %s\n", tmp.c_str(), path.c_str());
    return false;
  }
  return true;
}

std::string Config::channel_dir(const std::string &ch) const {
  return base_dir + "/" + ch;
}

std::string Config::game_binary(const std::string &ch) const {
#ifdef _WIN32
  return channel_dir(ch) + "/Silencer.exe";
#elif defined(__APPLE__)
  return channel_dir(ch) + "/Silencer.app/Contents/MacOS/Silencer";
#else
  return channel_dir(ch) + "/silencer";
#endif
}

const std::string &Config::installed(const std::string &ch) const {
  return ch == "nightly" ? installed_nightly : installed_stable;
}

void Config::set_installed(const std::string &ch, const std::string &version) {
  (ch == "nightly" ? installed_nightly : installed_stable) = version;
}

std::string game_data_dir() {
#ifdef _WIN32
  const char *appdata = getenv("APPDATA");
  if (!appdata || !appdata[0])
    return "";
  std::string d = std::string(appdata) + "/Silencer/";
  // os.cpp normalizes separators so concatenations stay uniform.
  for (char &c : d)
    if (c == '\\')
      c = '/';
  return d;
#elif defined(__APPLE__)
  const char *home = getenv("HOME");
  if (!home || !home[0])
    return "";
  return std::string(home) + "/Library/Application Support/Silencer/";
#else
  const char *home = getenv("HOME");
  if (!home || !home[0])
    return "";
  return std::string(home) + "/.config/silencer/";
#endif
}

} // namespace launcher
