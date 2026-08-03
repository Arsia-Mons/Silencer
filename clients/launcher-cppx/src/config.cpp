#include "config.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace launcher {

using nlohmann::json;

namespace {

std::string home_dir() {
  const char *h = getenv("HOME");
  return h && h[0] ? std::string(h) : std::string(".");
}

void mkdir_p(const std::string &path) {
  std::string cur;
  for (size_t i = 0; i < path.size(); ++i) {
    cur += path[i];
    if (path[i] == '/' && cur.size() > 1)
      mkdir(cur.c_str(), 0755);
  }
  mkdir(path.c_str(), 0755);
}

} // namespace

std::string Config::dir_path() {
  // SILENCER_LAUNCHER_CONFIG overrides the file path (for testing); the config
  // dir is then its parent. Default is the exact shared-contract location.
  const char *env = getenv("SILENCER_LAUNCHER_CONFIG");
  if (env && env[0]) {
    std::string p = env;
    size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
  }
  return home_dir() + "/.config/silencer-launcher";
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
  if (install_dir.empty())
    install_dir = home_dir() + "/.local/share/silencer-launcher";
  if (game_binary.empty())
    game_binary = install_dir + "/Silencer.app/Contents/MacOS/Silencer";
  if (servers.empty())
    servers.push_back({"Official", "lobby.arsiamons.com", 517});
  if (manifest_url_stable.empty())
    manifest_url_stable =
        "https://github.com/Arsia-Mons/Silencer/releases/latest/download/update.json";
  if (manifest_url_nightly.empty())
    manifest_url_nightly =
        "https://github.com/Arsia-Mons/Silencer/releases/download/latest/update.json";
  if (announcements_url.empty())
    announcements_url = "https://admin.arsiamons.com/api/announcements";
  if (last_server < 0 || last_server >= (int)servers.size())
    last_server = 0;
}

bool Config::load() {
  std::ifstream in(file_path());
  if (in) {
    try {
      json j;
      in >> j;
      // Read each field type-guarded: the config file is a shared contract with
      // the competing launcher, so a single mismatched field must not abort the
      // whole parse — fall back to the default for that field only.
      auto str = [&](const char *k, std::string &dst) {
        if (j.contains(k) && j[k].is_string())
          dst = j[k].get<std::string>();
      };
      str("channel", channel);
      str("installed_version", installed_version);
      str("install_dir", install_dir);
      str("game_binary", game_binary);
      str("manifest_url_stable", manifest_url_stable);
      str("manifest_url_nightly", manifest_url_nightly);
      str("announcements_url", announcements_url);
      if (j.contains("last_server") && j["last_server"].is_number_integer())
        last_server = j["last_server"].get<int>();
      if (j.contains("servers") && j["servers"].is_array()) {
        servers.clear();
        for (const auto &s : j["servers"]) {
          if (!s.is_object())
            continue;
          ServerCfg sc;
          if (s.contains("name") && s["name"].is_string())
            sc.name = s["name"].get<std::string>();
          else
            sc.name = "Server";
          if (s.contains("host") && s["host"].is_string())
            sc.host = s["host"].get<std::string>();
          if (s.contains("port") && s["port"].is_number_integer())
            sc.port = s["port"].get<int>();
          if (!sc.host.empty())
            servers.push_back(sc);
        }
      }
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
  j["installed_version"] = installed_version;
  j["install_dir"] = install_dir;
  j["game_binary"] = game_binary;
  j["last_server"] = last_server;
  j["manifest_url_stable"] = manifest_url_stable;
  j["manifest_url_nightly"] = manifest_url_nightly;
  j["announcements_url"] = announcements_url;
  j["servers"] = json::array();
  for (const auto &s : servers)
    j["servers"].push_back({{"name", s.name}, {"host", s.host}, {"port", s.port}});

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
  if (rename(tmp.c_str(), path.c_str()) != 0) {
    fprintf(stderr, "[launcher] cannot rename %s -> %s\n", tmp.c_str(), path.c_str());
    return false;
  }
  return true;
}

const ServerCfg *Config::selected_server() const {
  if (last_server < 0 || last_server >= (int)servers.size())
    return nullptr;
  return &servers[last_server];
}

} // namespace launcher
