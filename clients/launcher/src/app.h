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
enum class FetchStatus { Idle, Loading, Loaded, Empty, Failed };
enum class UpdateStatus { Idle, Downloading, Verifying, Extracting, Done, Failed };
enum class PingStatus { Unknown, Probing, Online, Offline };
enum class AuthStatus { SignedOut, Connecting, SignedIn, Failed };

// ---- News (portable block AST, version-2 feed; see shared/news/CLAUDE.md) --
struct NewsSpan {
  std::string text;
  bool bold = false;
  bool italic = false;
  std::string href; // "" = not a link
  bool operator==(const NewsSpan &) const = default;
};

struct NewsBlock {
  enum class Type { Paragraph, Heading, List };
  Type type = Type::Paragraph;
  int level = 2;        // Heading only
  bool ordered = false; // List only
  std::vector<NewsSpan> spans;              // Paragraph / Heading
  std::vector<std::vector<NewsSpan>> items; // List
  bool operator==(const NewsBlock &) const = default;
};

struct Announcement {
  std::string slug;
  std::string title;
  std::string date;
  bool pinned = false;
  std::vector<NewsBlock> blocks;
  bool operator==(const Announcement &) const = default;
};

// ---- Releases (GitHub Releases API) ----------------------------------------
struct Release {
  std::string version; // tag minus the leading "v"
  std::string date;    // YYYY-MM-DD
  bool prerelease = false;
  std::vector<std::string> notes; // cleaned "What's Changed" bullets
  bool operator==(const Release &) const = default;
};

// ---- Per-channel manifest/install state -------------------------------------
struct ChannelState {
  ManifestStatus manifest = ManifestStatus::Idle;
  std::string latest_version;
  std::string message;   // populated when Unavailable
  std::string installed; // "" = not installed
  bool binary_valid = false;
  bool operator==(const ChannelState &) const = default;
};

// An immutable, self-contained copy of everything the UI renders this frame.
// Built under lock by App::snapshot(); the UI never touches App internals.
struct AppSnapshot {
  std::string channel; // active channel
  std::string base_dir;
  ChannelState stable;
  ChannelState nightly;
  const ChannelState &chan(const std::string &ch) const {
    return ch == "nightly" ? nightly : stable;
  }
  const ChannelState &active() const { return chan(channel); }

  UpdateStatus update_status = UpdateStatus::Idle;
  std::string update_channel; // channel being installed while != Idle
  float update_progress = 0.0f;
  std::string update_error;

  FetchStatus news_status = FetchStatus::Idle;
  std::vector<Announcement> announcements; // already sorted (pinned, newest)

  FetchStatus releases_status = FetchStatus::Idle;
  std::vector<Release> releases_stable;  // newest first
  std::vector<Release> releases_nightly; // prereleases, newest first

  PingStatus ping = PingStatus::Unknown;
  int latency_ms = -1;
  std::string lobby_host;

  AuthStatus auth = AuthStatus::SignedOut;
  std::string auth_username; // signed-in (or attempting) username
  uint32_t auth_account_id = 0;
  std::string auth_error;

  // Deep equality over every member (deliberately defaulted so new fields are
  // included automatically) — the main loop's idle-frame-skip gate.
  bool operator==(const AppSnapshot &) const = default;
};

// Owns config + all network/process side effects. A single background worker
// thread serializes network work (manifest/news/releases fetch, ping, lobby
// auth, install/uninstall) off the UI thread; the UI polls snapshot() each
// frame and calls the intent methods, which are safe to call from the UI
// thread.
class App {
public:
  App();
  ~App();

  AppSnapshot snapshot();

  // --- Intents (call from the UI thread) ---
  void set_channel(const std::string &channel); // "stable" | "nightly"
  void refresh();                               // re-fetch manifests + feeds + re-ping
  void install(const std::string &channel);     // install or update the channel
  void uninstall(const std::string &channel);   // remove {base_dir}/{channel}
  void set_base_dir(const std::string &dir);    // takes effect for future installs
  void play();                                  // launch the active channel's game
  void sign_in(const std::string &username, const std::string &password);
  void sign_out();
  // The launcher's own updates are the bootstrap stub's job
  // (clients/launcher-stub/) — this process only ever updates the GAME.

private:
  struct Command {
    enum class Kind { Refresh, Install, Uninstall, SignIn } kind;
    std::string channel;
    std::string username;
    std::string password;
  };

  void worker_main();
  void run_refresh();
  void run_install(const std::string &channel);
  void run_uninstall(const std::string &channel);
  void run_sign_in(const std::string &username, const std::string &password);
  void enqueue(Command cmd);

  void refresh_installed_locked();

  // Fetch `url` into a temp file and read it back as a string. Empty on failure.
  std::string fetch_text(const std::string &url, const std::string &tmp_name,
                         int *http_status);

  Config config_;
  UpdaterDownload downloader_;

  std::mutex mtx_;
  std::condition_variable cv_;
  std::deque<Command> queue_;
  bool quit_ = false;
  std::thread worker_;

  // Readable state (guarded by mtx_ unless noted).
  ChannelState stable_;
  ChannelState nightly_;
  ChannelState &chan_locked(const std::string &ch) {
    return ch == "nightly" ? nightly_ : stable_;
  }
  std::string dl_url_stable_, dl_sha_stable_;
  std::string dl_url_nightly_, dl_sha_nightly_;

  FetchStatus news_status_ = FetchStatus::Idle;
  std::vector<Announcement> announcements_;

  FetchStatus releases_status_ = FetchStatus::Idle;
  std::vector<Release> releases_stable_;
  std::vector<Release> releases_nightly_;

  PingStatus ping_ = PingStatus::Unknown;
  int latency_ms_ = -1;

  AuthStatus auth_ = AuthStatus::SignedOut;
  std::string auth_username_;
  uint32_t auth_account_id_ = 0;
  std::string auth_error_;

  UpdateStatus update_status_ = UpdateStatus::Idle;
  std::string update_channel_;
  std::string update_error_;
  std::atomic<uint64_t> update_bytes_got_{0};
  std::atomic<uint64_t> update_bytes_total_{0};

  std::atomic<bool> cancel_{false};
  // Set by ~App before joining the worker. curl only unwinds a blocked
  // transfer from its progress callback, so without this the destructor waits
  // out the whole in-flight fetch — up to the 15s connect timeout per URL, and
  // unbounded on a stalled read. Atomic because the curl thread reads it
  // without mtx_ (which `quit_` needs).
  std::atomic<bool> shutdown_{false};

  friend bool app_download_progress(void *ctx, uint64_t got, uint64_t total);
  friend bool app_fetch_progress(void *ctx, uint64_t got, uint64_t total);
};

} // namespace launcher

#endif
