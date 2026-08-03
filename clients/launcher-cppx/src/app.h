#ifndef LAUNCHER_APP_H
#define LAUNCHER_APP_H

#include "config.h"
#include "updaterdownload.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace launcher {

enum class ManifestStatus { Idle, Loading, UpdateAvailable, UpToDate, Unavailable };
enum class NewsStatus { Idle, Loading, Loaded, Empty };
enum class UpdateStatus { Idle, Downloading, Verifying, Extracting, Done, Failed };
enum class PingStatus { Unknown, Probing, Online, Offline };

struct Announcement {
  std::string title;
  std::string body;
  std::string date;
  bool pinned = false;
};

struct ServerView {
  std::string name;
  std::string host;
  int port = 0;
  PingStatus ping = PingStatus::Unknown;
  int latency_ms = -1;
};

// An immutable, self-contained copy of everything the UI renders this frame.
// Built under lock by App::snapshot(); the UI never touches App internals.
struct AppSnapshot {
  std::string channel;
  std::string installed_version;

  ManifestStatus manifest_status = ManifestStatus::Idle;
  std::string manifest_version;   // latest available version for the channel
  std::string manifest_message;   // populated when Unavailable

  UpdateStatus update_status = UpdateStatus::Idle;
  float update_progress = 0.0f;   // 0..1 during Downloading
  std::string update_error;

  NewsStatus news_status = NewsStatus::Idle;
  std::vector<Announcement> announcements; // already sorted (pinned, newest)

  std::vector<ServerView> servers;
  int selected_server = 0;

  std::string game_binary;
  bool game_binary_valid = false;
};

// Owns config + all network/process side effects. A single background worker
// thread serializes network work (manifest/news fetch, pings, update download)
// off the UI thread; the UI polls snapshot() each frame and calls the intent
// methods, which are safe to call from the UI thread.
class App {
public:
  App();
  ~App();

  AppSnapshot snapshot();

  // --- Intents (call from the UI thread) ---
  void set_channel(const std::string &channel); // "stable" | "nightly"
  void refresh();                                // re-fetch manifest + news + re-ping
  void start_update();                           // download + verify + extract latest
  void select_server(int index);
  void play();

private:
  enum class Command { Refresh, Update };

  void worker_main();
  void run_refresh();
  void run_update();

  // Fetch `url` into a temp file and read it back as a string. Empty on failure.
  std::string fetch_text(const std::string &url, const std::string &tmp_name,
                         int *http_status);

  std::string manifest_url_locked() const; // caller holds mtx_

  Config config_;
  UpdaterDownload downloader_;

  std::mutex mtx_;
  std::condition_variable cv_;
  std::deque<Command> queue_;
  bool quit_ = false;
  std::thread worker_;

  // Readable state (guarded by mtx_ unless noted).
  ManifestStatus manifest_status_ = ManifestStatus::Idle;
  std::string manifest_version_;
  std::string manifest_download_url_;
  std::string manifest_sha256_;
  std::string manifest_message_;

  NewsStatus news_status_ = NewsStatus::Idle;
  std::vector<Announcement> announcements_;

  std::vector<ServerView> servers_;

  UpdateStatus update_status_ = UpdateStatus::Idle;
  std::string update_error_;
  std::atomic<uint64_t> update_bytes_got_{0};
  std::atomic<uint64_t> update_bytes_total_{0};
  std::atomic<bool> cancel_{false};

  friend bool app_download_progress(void *ctx, uint64_t got, uint64_t total);
};

} // namespace launcher

#endif
