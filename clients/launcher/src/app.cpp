#include "app.h"

#include "net.h"
#include "updaterzip.h"

#include "silencer/lobby/client.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace launcher {

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::string temp_dir() {
#ifdef _WIN32
  const char *t = getenv("TEMP");
  if (!t || !t[0])
    t = getenv("TMP");
  return (t && t[0]) ? std::string(t) : std::string(".");
#else
  return "/tmp";
#endif
}

std::string to_lower(std::string s) {
  for (char &c : s)
    c = (char)std::tolower((unsigned char)c);
  return s;
}

// The baked glyph atlases cover ASCII 33..126 only, so multi-byte UTF-8 (and
// upper Latin-1) renders blank. Fold feed/release text to renderable bytes:
// common typographic punctuation maps to ASCII lookalikes, the rest drops.
std::string fold_glyphs(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
      out += (char)c;
      ++i;
      continue;
    }
    uint32_t cp = 0;
    size_t len = 1;
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
      cp = ((uint32_t)(c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
      len = 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
      cp = ((uint32_t)(c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) |
           ((unsigned char)s[i + 2] & 0x3F);
      len = 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
      len = 4;
    }
    switch (cp) {
    case 0x2013: case 0x2014: out += '-'; break;   // – —
    case 0x2018: case 0x2019: out += '\''; break;  // ‘ ’
    case 0x201C: case 0x201D: out += '"'; break;   // “ ”
    case 0x2026: out += "..."; break;              // …
    case 0x2192: out += "->"; break;               // →
    case 0x00D7: out += 'x'; break;                // ×
    case 0x00B7: case 0x2022: out += '-'; break;   // · • (bullets)
    default:
      if (cp >= 0x21 && cp <= 0x7E)
        out += (char)cp; // within atlas coverage
      break;             // everything else drops
    }
    i += len;
  }
  return out;
}

// ---- News feed (version 2 block AST; see shared/news/CLAUDE.md) ------------

std::vector<NewsSpan> parse_spans(const json &arr) {
  std::vector<NewsSpan> out;
  if (!arr.is_array())
    return out;
  for (const auto &s : arr) {
    if (!s.is_object() || !s.contains("text") || !s["text"].is_string())
      continue;
    NewsSpan sp;
    sp.text = fold_glyphs(s["text"].get<std::string>());
    sp.bold = s.value("bold", false);
    sp.italic = s.value("italic", false);
    sp.href = s.value("href", std::string());
    out.push_back(std::move(sp));
  }
  return out;
}

std::vector<Announcement> parse_news_v2(const std::string &body) {
  std::vector<Announcement> out;
  json j = json::parse(body); // caller catches
  if (j.value("version", 0) != 2 || !j.contains("items") || !j["items"].is_array())
    return out;
  for (const auto &it : j["items"]) {
    if (!it.is_object())
      continue;
    Announcement a;
    a.slug = it.value("slug", std::string());
    a.title = fold_glyphs(it.value("title", std::string()));
    a.date = it.value("date", std::string());
    a.pinned = it.value("pinned", false);
    if (it.contains("blocks") && it["blocks"].is_array()) {
      for (const auto &b : it["blocks"]) {
        if (!b.is_object())
          continue;
        const std::string type = b.value("type", std::string());
        NewsBlock blk;
        if (type == "paragraph") {
          blk.type = NewsBlock::Type::Paragraph;
          blk.spans = parse_spans(b.contains("spans") ? b["spans"] : json());
        } else if (type == "heading") {
          blk.type = NewsBlock::Type::Heading;
          blk.level = b.value("level", 2);
          blk.spans = parse_spans(b.contains("spans") ? b["spans"] : json());
        } else if (type == "list") {
          blk.type = NewsBlock::Type::List;
          blk.ordered = b.value("ordered", false);
          if (b.contains("items") && b["items"].is_array())
            for (const auto &li : b["items"])
              blk.items.push_back(parse_spans(li));
        } else {
          continue; // unknown block types are skipped, not fatal
        }
        a.blocks.push_back(std::move(blk));
      }
    }
    if (!a.title.empty())
      out.push_back(std::move(a));
  }
  // The compiler pre-sorts, but don't rely on it: pinned first, then date desc.
  std::stable_sort(out.begin(), out.end(), [](const Announcement &a, const Announcement &b) {
    if (a.pinned != b.pinned)
      return a.pinned;
    return a.date > b.date;
  });
  return out;
}

// ---- GitHub releases --------------------------------------------------------

// One markdown bullet -> plain text: "[t](u)" -> "t", "**" stripped, the
// auto-generated " by @user in https://..." trailer dropped.
std::string clean_bullet(std::string s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    if (s[i] == '[') {
      size_t close = s.find("](", i);
      size_t paren = close == std::string::npos ? std::string::npos : s.find(')', close + 2);
      if (close != std::string::npos && paren != std::string::npos) {
        out += s.substr(i + 1, close - i - 1);
        i = paren + 1;
        continue;
      }
    }
    if (s.compare(i, 2, "**") == 0) {
      i += 2;
      continue;
    }
    out += s[i++];
  }
  size_t by = out.find(" by @");
  if (by != std::string::npos)
    out.erase(by);
  while (!out.empty() && (out.back() == ' ' || out.back() == '\r'))
    out.pop_back();
  return fold_glyphs(out);
}

std::vector<std::string> release_notes(const std::string &body) {
  // Prefer bullets under "## What's Changed" (the auto-generated section);
  // if that heading is absent, take every bullet in the body.
  const bool has_changes = body.find("## What's Changed") != std::string::npos;
  std::vector<std::string> notes;
  std::istringstream in(body);
  std::string line;
  bool in_changes = !has_changes;
  while (std::getline(in, line)) {
    if (line.rfind("## ", 0) == 0) {
      in_changes = !has_changes || line.find("What's Changed") != std::string::npos;
      continue;
    }
    if (!in_changes)
      continue;
    if (line.rfind("* ", 0) == 0 || line.rfind("- ", 0) == 0) {
      std::string b = clean_bullet(line.substr(2));
      if (!b.empty())
        notes.push_back(std::move(b));
    }
  }
  return notes;
}

void parse_releases(const std::string &body, std::vector<Release> *stable,
                    std::vector<Release> *nightly) {
  json j = json::parse(body); // caller catches
  if (!j.is_array())
    return;
  for (const auto &r : j) {
    if (!r.is_object() || r.value("draft", false))
      continue;
    Release rel;
    std::string tag = r.value("tag_name", std::string());
    std::string name = r.value("name", std::string());
    rel.prerelease = r.value("prerelease", false);
    // Stable tags are "vNNNNN"; the nightly prerelease tag is just "latest",
    // whose display name ("Latest master (sha)") is the better label.
    rel.version = (!tag.empty() && tag[0] == 'v') ? tag.substr(1)
                  : (rel.prerelease && !name.empty()) ? name
                                                      : tag;
    std::string ts = r.value("published_at", std::string());
    rel.date = ts.size() >= 10 ? ts.substr(0, 10) : ts;
    rel.notes = release_notes(r.value("body", std::string()));
    if (rel.version.empty())
      continue;
    (rel.prerelease ? nightly : stable)->push_back(std::move(rel));
  }
  // GitHub returns newest-first already; keep deterministic anyway.
  auto newer = [](const Release &a, const Release &b) { return a.date > b.date; };
  std::stable_sort(stable->begin(), stable->end(), newer);
  std::stable_sort(nightly->begin(), nightly->end(), newer);
}

} // namespace

bool app_download_progress(void *ctx, uint64_t got, uint64_t total) {
  App *app = static_cast<App *>(ctx);
  app->update_bytes_got_.store(got);
  app->update_bytes_total_.store(total);
  return !app->cancel_.load() && !app->shutdown_.load();
}

// Metadata fetches have no progress UI, but they still need the abort hook:
// it's the only way curl lets go of a transfer that's mid-flight when the
// window closes.
bool app_fetch_progress(void *ctx, uint64_t, uint64_t) {
  return !static_cast<App *>(ctx)->shutdown_.load();
}

App::App() {
  config_.load();
  refresh_installed_locked(); // no contention yet: worker not started
  worker_ = std::thread(&App::worker_main, this);
  refresh();
}

App::~App() {
  // Order matters: the abort flags must be visible to the curl thread before
  // we block in join(), or the worker keeps waiting on the network.
  shutdown_.store(true);
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
    switch (cmd.kind) {
    case Command::Kind::Refresh:
      run_refresh();
      break;
    case Command::Kind::Install:
      run_install(cmd.channel);
      break;
    case Command::Kind::Uninstall:
      run_uninstall(cmd.channel);
      break;
    case Command::Kind::SignIn:
      run_sign_in(cmd.username, cmd.password);
      break;
    }
  }
}

std::string App::fetch_text(const std::string &url, const std::string &tmp_name,
                            int *http_status) {
  const std::string path = temp_dir() + "/" + tmp_name;
  int http = 0;
  std::string err;
  UpdaterDownload::Result r =
      downloader_.Fetch(url, path, app_fetch_progress, this, &http, &err);
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

// Recompute installed versions + binary validity from config. Caller holds
// mtx_ (or is single-threaded during construction).
void App::refresh_installed_locked() {
  for (const char *ch : {"stable", "nightly"}) {
    ChannelState &cs = chan_locked(ch);
    cs.installed = config_.installed(ch);
    cs.binary_valid = is_executable_file(config_.game_binary(ch));
  }
}

void App::run_refresh() {
  std::string lobby_host, news_url, releases_url;
  std::string manifest_urls[2];
  int lobby_port = 0;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    stable_.manifest = ManifestStatus::Loading;
    nightly_.manifest = ManifestStatus::Loading;
    news_status_ = FetchStatus::Loading;
    releases_status_ = FetchStatus::Loading;
    ping_ = PingStatus::Probing;
    manifest_urls[0] = config_.manifest_url_stable;
    manifest_urls[1] = config_.manifest_url_nightly;
    news_url = config_.announcements_url;
    releases_url = config_.releases_url;
    lobby_host = config_.lobby_host;
    lobby_port = config_.lobby_port;
  }

  // --- Manifests (both channels; the drop-up shows per-channel status) ---
  const char *channels[2] = {"stable", "nightly"};
  for (int i = 0; i < 2; ++i) {
    int http = 0;
    std::string body =
        fetch_text(manifest_urls[i], std::string("silencer-launcher-manifest-") + channels[i] + ".json",
                   &http);
    std::string version, dl_url, sha;
    if (!body.empty()) {
      try {
        json j = json::parse(body);
        // `version` is the wire protocol number; the lobby compares it against
        // every connecting client, so it must stay the protocol number and
        // cannot also carry a per-build identity. `build_id` is that identity:
        // two nightlies share a protocol number but never a build_id, which is
        // what makes nightly-to-nightly updates detectable. Manifests published
        // before build_id existed carry only `version`, so fall back to it.
        version = j.value("build_id", std::string());
        if (version.empty())
          version = j.value("version", std::string());
#ifdef _WIN32
        dl_url = j.value("windows_url", std::string());
        sha = to_lower(j.value("windows_sha256", std::string()));
#else
        dl_url = j.value("macos_url", std::string());
        sha = to_lower(j.value("macos_sha256", std::string()));
#endif
      } catch (const std::exception &) {
        version.clear();
      }
    }
    std::lock_guard<std::mutex> lk(mtx_);
    ChannelState &cs = chan_locked(channels[i]);
    std::string &dl = (i == 0) ? dl_url_stable_ : dl_url_nightly_;
    std::string &dsha = (i == 0) ? dl_sha_stable_ : dl_sha_nightly_;
    if (!version.empty()) {
      cs.latest_version = version;
      dl = dl_url;
      dsha = sha;
      const bool differs = version != config_.installed(channels[i]);
      cs.manifest = (differs && !dl_url.empty()) ? ManifestStatus::UpdateAvailable
                                                 : ManifestStatus::UpToDate;
      cs.message.clear();
    } else {
      cs.manifest = ManifestStatus::Unavailable;
      cs.message = std::string("Manifest unavailable for ") + channels[i];
    }
  }

  // --- News (version-2 block feed) ---
  std::vector<Announcement> anns;
  bool news_ok = false;
  {
    int http = 0;
    std::string body = fetch_text(news_url, "silencer-launcher-news.json", &http);
    if (!body.empty()) {
      try {
        anns = parse_news_v2(body);
        news_ok = true;
      } catch (const std::exception &) {
      }
    }
  }
  {
    std::lock_guard<std::mutex> lk(mtx_);
    announcements_ = std::move(anns);
    news_status_ = !news_ok            ? FetchStatus::Failed
                   : announcements_.empty() ? FetchStatus::Empty
                                            : FetchStatus::Loaded;
  }

  // --- Releases (GitHub API) ---
  std::vector<Release> rel_stable, rel_nightly;
  bool rel_ok = false;
  {
    int http = 0;
    std::string body = fetch_text(releases_url, "silencer-launcher-releases.json", &http);
    if (!body.empty()) {
      try {
        parse_releases(body, &rel_stable, &rel_nightly);
        rel_ok = true;
      } catch (const std::exception &) {
      }
    }
  }
  {
    std::lock_guard<std::mutex> lk(mtx_);
    releases_stable_ = std::move(rel_stable);
    releases_nightly_ = std::move(rel_nightly);
    releases_status_ = !rel_ok ? FetchStatus::Failed
                       : (releases_stable_.empty() && releases_nightly_.empty())
                           ? FetchStatus::Empty
                           : FetchStatus::Loaded;
  }

  // --- Lobby ping ---
  if (cancel_.load())
    return;
  int ms = tcp_ping(lobby_host, lobby_port, 3000);
  {
    std::lock_guard<std::mutex> lk(mtx_);
    ping_ = (ms >= 0) ? PingStatus::Online : PingStatus::Offline;
    latency_ms_ = ms;
  }
}

void App::run_install(const std::string &channel) {
  std::string dl_url, expected_sha, dest, version;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (update_status_ == UpdateStatus::Downloading)
      return;
    ChannelState &cs = chan_locked(channel);
    dl_url = (channel == "nightly") ? dl_url_nightly_ : dl_url_stable_;
    expected_sha = (channel == "nightly") ? dl_sha_nightly_ : dl_sha_stable_;
    version = cs.latest_version;
    dest = config_.channel_dir(channel);
    if (dl_url.empty() || version.empty()) {
      update_status_ = UpdateStatus::Failed;
      update_channel_ = channel;
      update_error_ = "No download available for " + channel;
      return;
    }
    update_status_ = UpdateStatus::Downloading;
    update_channel_ = channel;
    update_error_.clear();
    update_bytes_got_.store(0);
    update_bytes_total_.store(0);
  }

  const std::string zip = temp_dir() + "/silencer-launcher-" + channel + ".zip";
  int http = 0;
  std::string err;
  UpdaterDownload::Result r = downloader_.Fetch(dl_url, zip, app_download_progress, this, &http, &err);
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
    update_error_ = "Checksum mismatch - download discarded";
    return;
  }

  {
    std::lock_guard<std::mutex> lk(mtx_);
    update_status_ = UpdateStatus::Extracting;
  }
  std::error_code ec;
  fs::create_directories(dest, ec);
  UpdaterZip::Result zr = UpdaterZip::Extract(zip, dest);
  remove(zip.c_str());
  if (zr != UpdaterZip::OK) {
    std::lock_guard<std::mutex> lk(mtx_);
    update_status_ = UpdateStatus::Failed;
    update_error_ = "Could not extract the download";
    return;
  }

  {
    std::lock_guard<std::mutex> lk(mtx_);
    config_.set_installed(channel, version);
    config_.save();
    refresh_installed_locked();
    ChannelState &cs = chan_locked(channel);
    cs.manifest = ManifestStatus::UpToDate;
    update_status_ = UpdateStatus::Done;
    update_channel_.clear();
  }
}

void App::run_uninstall(const std::string &channel) {
  std::string dir;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (update_status_ == UpdateStatus::Downloading)
      return;
    dir = config_.channel_dir(channel);
  }
  std::error_code ec;
  fs::remove_all(dir, ec); // best-effort; a locked file leaves leftovers
  {
    std::lock_guard<std::mutex> lk(mtx_);
    config_.set_installed(channel, "");
    config_.save();
    refresh_installed_locked();
    ChannelState &cs = chan_locked(channel);
    if (cs.manifest == ManifestStatus::UpToDate && !cs.latest_version.empty())
      cs.manifest = ManifestStatus::UpdateAvailable;
  }
}

void App::run_sign_in(const std::string &username, const std::string &password) {
  std::string host;
  int port = 0;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    host = config_.lobby_host;
    port = config_.lobby_port;
    auth_ = AuthStatus::Connecting;
    auth_username_ = username;
    auth_error_.clear();
  }

  silencer::lobby::ClientConfig cfg;
  cfg.host = host;
  cfg.port = (uint16_t)port;
  cfg.version = ""; // skip the version handshake; auth is all we need
  silencer::lobby::Client client(cfg);

  bool done = false, ok = false;
  uint32_t account_id = 0;
  std::string error;
  client.on_auth([&](const silencer::lobby::AuthResult &a) {
    done = true;
    ok = a.ok;
    account_id = a.account_id;
    error = a.error;
  });

  client.connect();
  bool sent = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!done && !cancel_.load() && std::chrono::steady_clock::now() < deadline) {
    if (!client.poll(std::chrono::milliseconds(100)))
      break;
    if (!sent && client.state() == silencer::lobby::ConnectionState::AwaitingAuth) {
      client.send_credentials(username, password);
      sent = true;
    }
  }
  if (!done && error.empty())
    error = client.last_error().empty() ? "Could not reach the lobby" : client.last_error();
  client.disconnect();

  std::lock_guard<std::mutex> lk(mtx_);
  if (ok) {
    auth_ = AuthStatus::SignedIn;
    auth_account_id_ = account_id;
    auth_error_.clear();
  } else {
    auth_ = AuthStatus::Failed;
    auth_account_id_ = 0;
    auth_error_ = error.empty() ? "Sign-in failed" : error;
  }
}

AppSnapshot App::snapshot() {
  std::lock_guard<std::mutex> lk(mtx_);
  AppSnapshot s;
  s.channel = config_.channel;
  s.base_dir = config_.base_dir;
  s.stable = stable_;
  s.nightly = nightly_;
  s.update_status = update_status_;
  s.update_channel = update_channel_;
  {
    const uint64_t total = update_bytes_total_.load();
    const uint64_t got = update_bytes_got_.load();
    s.update_progress = total ? std::min(1.0f, (float)((double)got / (double)total)) : 0.0f;
  }
  s.update_error = update_error_;
  s.news_status = news_status_;
  s.announcements = announcements_;
  s.releases_status = releases_status_;
  s.releases_stable = releases_stable_;
  s.releases_nightly = releases_nightly_;
  s.ping = ping_;
  s.latency_ms = latency_ms_;
  s.lobby_host = config_.lobby_host;
  s.auth = auth_;
  s.auth_username = auth_username_;
  s.auth_account_id = auth_account_id_;
  s.auth_error = auth_error_;
  return s;
}

void App::enqueue(Command cmd) {
  {
    std::lock_guard<std::mutex> lk(mtx_);
    queue_.push_back(std::move(cmd));
  }
  cv_.notify_all();
}

void App::set_channel(const std::string &channel) {
  const std::string norm = (channel == "nightly") ? "nightly" : "stable";
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (config_.channel == norm)
      return;
    config_.channel = norm;
    config_.save();
  }
}

void App::refresh() { enqueue({Command::Kind::Refresh}); }

void App::install(const std::string &channel) {
  Command c{Command::Kind::Install};
  c.channel = (channel == "nightly") ? "nightly" : "stable";
  enqueue(std::move(c));
}

void App::uninstall(const std::string &channel) {
  Command c{Command::Kind::Uninstall};
  c.channel = (channel == "nightly") ? "nightly" : "stable";
  enqueue(std::move(c));
}

void App::set_base_dir(const std::string &dir) {
  std::string d = dir;
  while (!d.empty() && (d.back() == '/' || d.back() == '\\'))
    d.pop_back();
  if (d.empty())
    return;
  std::lock_guard<std::mutex> lk(mtx_);
  if (config_.base_dir == d)
    return;
  config_.base_dir = d;
  config_.save();
  refresh_installed_locked();
}

void App::play() {
  std::string binary;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    binary = config_.game_binary(config_.channel);
  }
  spawn_detached(binary, {});
}

void App::sign_in(const std::string &username, const std::string &password) {
  if (username.empty() || password.empty())
    return;
  Command c{Command::Kind::SignIn};
  c.username = username;
  c.password = password;
  enqueue(std::move(c));
}

void App::sign_out() {
  std::lock_guard<std::mutex> lk(mtx_);
  auth_ = AuthStatus::SignedOut;
  auth_username_.clear();
  auth_account_id_ = 0;
  auth_error_.clear();
}

} // namespace launcher
