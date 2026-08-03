#include "app.h"

#include "net.h"
#include "updaterzip.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace launcher {

using nlohmann::json;

namespace {

void mkdir_p(const std::string &path) {
  std::string cur;
  for (size_t i = 0; i < path.size(); ++i) {
    cur += path[i];
    if (path[i] == '/' && cur.size() > 1)
      mkdir(cur.c_str(), 0755);
  }
  mkdir(path.c_str(), 0755);
}

std::string to_lower(std::string s) {
  for (char &c : s)
    c = (char)std::tolower((unsigned char)c);
  return s;
}

} // namespace

bool app_download_progress(void *ctx, uint64_t got, uint64_t total) {
  App *app = static_cast<App *>(ctx);
  app->update_bytes_got_.store(got);
  app->update_bytes_total_.store(total);
  return !app->cancel_.load();
}

App::App() {
  config_.load();
  for (const auto &s : config_.servers)
    servers_.push_back({s.name, s.host, s.port, PingStatus::Unknown, -1});
  worker_ = std::thread(&App::worker_main, this);
  refresh();
}

App::~App() {
  cancel_.store(true);
  {
    std::lock_guard<std::mutex> lk(mtx_);
    quit_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable())
    worker_.join();
}

void App::worker_main() {
  for (;;) {
    Command cmd;
    {
      std::unique_lock<std::mutex> lk(mtx_);
      cv_.wait(lk, [&] { return quit_ || !queue_.empty(); });
      if (quit_)
        return;
      cmd = queue_.front();
      queue_.pop_front();
    }
    switch (cmd) {
    case Command::Refresh:
      run_refresh();
      break;
    case Command::Update:
      run_update();
      break;
    }
  }
}

std::string App::manifest_url_locked() const {
  return config_.channel == "nightly" ? config_.manifest_url_nightly
                                      : config_.manifest_url_stable;
}

std::string App::fetch_text(const std::string &url, const std::string &tmp_name,
                            int *http_status) {
  const std::string path = std::string("/tmp/") + tmp_name;
  int http = 0;
  std::string err;
  UpdaterDownload::Result r =
      downloader_.Fetch(url, path, nullptr, nullptr, &http, &err);
  if (http_status)
    *http_status = http;
  if (r != UpdaterDownload::OK)
    return "";
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return "";
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void App::run_refresh() {
  std::string url, channel;
  std::vector<ServerView> servers_copy;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    manifest_status_ = ManifestStatus::Loading;
    news_status_ = NewsStatus::Loading;
    for (auto &s : servers_)
      s.ping = PingStatus::Probing;
    url = manifest_url_locked();
    channel = config_.channel;
    servers_copy = servers_;
  }

  // --- Manifest ---
  int http = 0;
  std::string body = fetch_text(url, "silencer-launcher-manifest.json", &http);
  bool ok = false;
  std::string version, dl_url, sha;
  if (!body.empty()) {
    try {
      json j = json::parse(body);
      version = j.value("version", std::string());
      dl_url = j.value("macos_url", std::string());
      sha = to_lower(j.value("macos_sha256", std::string()));
      ok = !version.empty();
    } catch (const std::exception &) {
      ok = false;
    }
  }
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (ok) {
      manifest_version_ = version;
      manifest_download_url_ = dl_url;
      manifest_sha256_ = sha;
      const bool differs = version != config_.installed_version;
      manifest_status_ = (differs && !dl_url.empty()) ? ManifestStatus::UpdateAvailable
                                                      : ManifestStatus::UpToDate;
      manifest_message_.clear();
    } else {
      manifest_status_ = ManifestStatus::Unavailable;
      manifest_message_ = "Manifest unavailable for " + channel;
    }
  }

  // --- News ---
  std::string news_url;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    news_url = config_.announcements_url;
  }
  std::vector<Announcement> anns;
  std::string news_body = fetch_text(news_url, "silencer-launcher-news.json", &http);
  if (!news_body.empty()) {
    try {
      json j = json::parse(news_body);
      if (j.is_array()) {
        for (const auto &a : j) {
          Announcement ann;
          ann.title = a.value("title", std::string());
          ann.body = a.value("body", std::string());
          ann.date = a.value("date", std::string());
          ann.pinned = a.value("pinned", false);
          if (!ann.title.empty() || !ann.body.empty())
            anns.push_back(std::move(ann));
        }
      }
    } catch (const std::exception &) {
      anns.clear();
    }
  }
  // Pinned first, then newest first (dates are ISO-ish, so lexical desc works).
  std::stable_sort(anns.begin(), anns.end(), [](const Announcement &a, const Announcement &b) {
    if (a.pinned != b.pinned)
      return a.pinned;
    return a.date > b.date;
  });
  {
    std::lock_guard<std::mutex> lk(mtx_);
    announcements_ = std::move(anns);
    news_status_ = announcements_.empty() ? NewsStatus::Empty : NewsStatus::Loaded;
  }

  // --- Pings (sequential; each result is published as it lands) ---
  for (size_t i = 0; i < servers_copy.size(); ++i) {
    if (cancel_.load())
      return;
    int ms = tcp_ping(servers_copy[i].host, servers_copy[i].port, 3000);
    std::lock_guard<std::mutex> lk(mtx_);
    if (i < servers_.size()) {
      servers_[i].ping = (ms >= 0) ? PingStatus::Online : PingStatus::Offline;
      servers_[i].latency_ms = ms;
    }
  }
}

void App::run_update() {
  std::string dl_url, expected_sha, install_dir, version;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (manifest_status_ != ManifestStatus::UpdateAvailable ||
        update_status_ == UpdateStatus::Downloading) {
      return;
    }
    dl_url = manifest_download_url_;
    expected_sha = manifest_sha256_;
    install_dir = config_.install_dir;
    version = manifest_version_;
    update_status_ = UpdateStatus::Downloading;
    update_error_.clear();
    update_bytes_got_.store(0);
    update_bytes_total_.store(0);
  }

  const std::string zip = "/tmp/silencer-launcher-update.zip";
  int http = 0;
  std::string err;
  UpdaterDownload::Result r =
      downloader_.Fetch(dl_url, zip, app_download_progress, this, &http, &err);
  if (r != UpdaterDownload::OK) {
    std::lock_guard<std::mutex> lk(mtx_);
    update_status_ = UpdateStatus::Failed;
    update_error_ = err.empty() ? "Download failed" : err;
    return;
  }

  {
    std::lock_guard<std::mutex> lk(mtx_);
    update_status_ = UpdateStatus::Verifying;
  }
  const std::string got_sha = sha256_file_hex(zip);
  if (got_sha.empty() || (!expected_sha.empty() && got_sha != expected_sha)) {
    remove(zip.c_str());
    std::lock_guard<std::mutex> lk(mtx_);
    update_status_ = UpdateStatus::Failed;
    update_error_ = "Checksum mismatch — update discarded";
    return;
  }

  {
    std::lock_guard<std::mutex> lk(mtx_);
    update_status_ = UpdateStatus::Extracting;
  }
  mkdir_p(install_dir);
  UpdaterZip::Result zr = UpdaterZip::Extract(zip, install_dir);
  remove(zip.c_str());
  if (zr != UpdaterZip::OK) {
    std::lock_guard<std::mutex> lk(mtx_);
    update_status_ = UpdateStatus::Failed;
    update_error_ = "Could not extract the update";
    return;
  }

  {
    std::lock_guard<std::mutex> lk(mtx_);
    config_.installed_version = version;
    config_.save();
    update_status_ = UpdateStatus::Done;
    manifest_status_ = ManifestStatus::UpToDate;
  }
}

AppSnapshot App::snapshot() {
  std::lock_guard<std::mutex> lk(mtx_);
  AppSnapshot s;
  s.channel = config_.channel;
  s.installed_version = config_.installed_version;
  s.manifest_status = manifest_status_;
  s.manifest_version = manifest_version_;
  s.manifest_message = manifest_message_;
  s.update_status = update_status_;
  {
    const uint64_t total = update_bytes_total_.load();
    const uint64_t got = update_bytes_got_.load();
    s.update_progress = total ? std::min(1.0f, (float)((double)got / (double)total)) : 0.0f;
  }
  s.update_error = update_error_;
  s.news_status = news_status_;
  s.announcements = announcements_;
  s.servers = servers_;
  s.selected_server = config_.last_server;
  s.game_binary = config_.game_binary;
  s.game_binary_valid = is_executable_file(config_.game_binary);
  return s;
}

void App::set_channel(const std::string &channel) {
  const std::string norm = (channel == "nightly") ? "nightly" : "stable";
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (config_.channel == norm)
      return;
    config_.channel = norm;
    config_.save();
    manifest_status_ = ManifestStatus::Idle;
    manifest_message_.clear();
  }
  refresh();
}

void App::refresh() {
  {
    std::lock_guard<std::mutex> lk(mtx_);
    queue_.push_back(Command::Refresh);
  }
  cv_.notify_all();
}

void App::start_update() {
  {
    std::lock_guard<std::mutex> lk(mtx_);
    queue_.push_back(Command::Update);
  }
  cv_.notify_all();
}

void App::select_server(int index) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (index >= 0 && index < (int)config_.servers.size()) {
    config_.last_server = index;
    config_.save();
  }
}

void App::play() {
  std::string binary, host;
  int port = 0;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    binary = config_.game_binary;
    const ServerCfg *srv = config_.selected_server();
    if (srv) {
      host = srv->host;
      port = srv->port;
    }
  }
  if (host.empty())
    return;
  char portstr[16];
  snprintf(portstr, sizeof(portstr), "%d", port);
  spawn_detached(binary, {"--lobby-host", host, "--lobby-port", portstr});
}

} // namespace launcher
