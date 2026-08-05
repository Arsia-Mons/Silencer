// Portable flow: window at t=0 -> manifest check -> (maybe) update into the
// versioned store -> launch the current payload. Every failure lands on
// "launch the existing version"; the atomic current.txt flip is the only
// commit point, so a half-finished update can never break the install.

#include "stub.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace {

fs::path P(const std::string &utf8) { return fs::u8path(utf8); }
std::string S(const fs::path &p) { return p.u8string(); }

// ---------------- logging ----------------
std::ofstream g_log;

void log_open(const fs::path &path) {
  std::error_code ec;
  if (fs::exists(path, ec) && fs::file_size(path, ec) > 256 * 1024) {
    fs::path old = path;
    old += ".old";
    fs::rename(path, old, ec);
  }
  g_log.open(path, std::ios::app);
}

// ---------------- small file helpers ----------------
std::string read_line(const fs::path &p) {
  std::ifstream in(p);
  std::string s;
  std::getline(in, s);
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
    s.pop_back();
  return s;
}

bool write_atomic(const fs::path &p, const std::string &text) {
  fs::path tmp = p;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out)
      return false;
    out << text << "\n";
    out.flush();
    if (!out)
      return false;
  }
  std::error_code ec;
  fs::rename(tmp, p, ec); // replaces an existing file on MSVC and POSIX
  return !ec;
}

// ---------------- the store ----------------
struct Store {
  fs::path root; // <store>/<build-id>/ per version + current.txt/previous.txt
  fs::path current_file() const { return root / "current.txt"; }
  fs::path previous_file() const { return root / "previous.txt"; }
  fs::path version_dir(const std::string &id) const { return root / id; }
  fs::path staging_dir() const { return root / "staging"; }
};

#ifdef _WIN32
const char *kPayloadRel = "silencer-launcher.exe";
const char *kPlat = "windows";
#elif defined(__APPLE__)
const char *kPayloadRel = "Silencer Launcher.app/Contents/MacOS/Silencer Launcher";
const char *kPlat = "macos";
#else
const char *kPayloadRel = "silencer-launcher";
const char *kPlat = "linux";
#endif

fs::path exe_dir() { return P(stub::own_exe_path()).parent_path(); }

bool dir_writable(const fs::path &d) {
  std::error_code ec;
  fs::create_directories(d, ec);
  if (ec)
    return false;
  fs::path probe = d / ".probe";
  {
    std::ofstream out(probe);
    if (!out)
      return false;
  }
  fs::remove(probe, ec);
  return true;
}

fs::path user_data_dir() {
#ifdef _WIN32
  std::string base = stub::env_str("LOCALAPPDATA");
  if (base.empty())
    base = stub::env_str("USERPROFILE") + "\\AppData\\Local";
  return P(base) / "Silencer Launcher";
#elif defined(__APPLE__)
  return P(stub::env_str("HOME")) / "Library/Application Support/Silencer Launcher";
#else
  std::string xdg = stub::env_str("XDG_DATA_HOME");
  if (!xdg.empty())
    return P(xdg) / "silencer-launcher";
  return P(stub::env_str("HOME")) / ".local/share/silencer-launcher";
#endif
}

Store resolve_store() {
  std::string env = stub::env_str("SILENCER_STUB_STORE");
  if (!env.empty())
    return {P(env)};
#ifdef __APPLE__
  // The bundle in /Applications is signed and often not writable; versions
  // always live in Application Support.
  return {user_data_dir() / "versions"};
#else
  fs::path beside = exe_dir() / "versions";
  if (dir_writable(beside))
    return {beside};
  return {user_data_dir() / "versions"};
#endif
}

// ---------------- config ----------------
fs::path launcher_config_path() {
#ifdef _WIN32
  return P(stub::env_str("APPDATA")) / "Silencer Launcher/launcher.json";
#elif defined(__APPLE__)
  return P(stub::env_str("HOME")) /
         "Library/Application Support/Silencer Launcher/launcher.json";
#else
  std::string xdg = stub::env_str("XDG_CONFIG_HOME");
  fs::path base = xdg.empty() ? P(stub::env_str("HOME")) / ".config" : P(xdg);
  return base / "silencer-launcher/launcher.json";
#endif
}

std::string manifest_url() {
  std::string env = stub::env_str("SILENCER_STUB_MANIFEST_URL");
  if (!env.empty())
    return env;
  std::ifstream in(launcher_config_path());
  if (in) {
    std::stringstream ss;
    ss << in.rdbuf();
    std::string url = stub::json_str_field(ss.str(), "manifest_url_launcher");
    if (!url.empty())
      return url;
  }
  return "https://github.com/Arsia-Mons/Silencer/releases/latest/download/"
         "update-launcher.json";
}

// https anywhere; plain http only on loopback (local testing).
bool url_allowed(const std::string &url) {
  if (url.rfind("https://", 0) == 0)
    return true;
  if (url.rfind("http://", 0) != 0)
    return false;
  std::string host = url.substr(7);
  size_t cut = host.find_first_of(":/");
  if (cut != std::string::npos)
    host = host.substr(0, cut);
  return host == "localhost" || host == "127.0.0.1" || host == "[::1]";
}

// A build id becomes a directory name; refuse anything that isn't a
// plain token.
bool safe_id(const std::string &id) {
  if (id.empty() || id.size() > 64)
    return false;
  for (char c : id) {
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') || c == '.' || c == '_' || c == '-';
    if (!ok)
      return false;
  }
  return id != "." && id != ".." && id != "staging";
}

bool ends_with(const std::string &s, const std::string &tail) {
  return s.size() >= tail.size() &&
         s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

bool extract_archive(const fs::path &archive, const fs::path &into) {
  std::error_code ec;
  fs::create_directories(into, ec);
  const std::string a = S(archive), d = S(into);
  const bool targz = ends_with(a, ".tar.gz") || ends_with(a, ".tgz");
#ifdef _WIN32
  std::string tar = stub::env_str("SystemRoot") + "\\System32\\tar.exe";
  return stub::run_tool({tar, "-xf", a, "-C", d});
#elif defined(__APPLE__)
  if (targz)
    return stub::run_tool({"/usr/bin/tar", "-xzf", a, "-C", d});
  return stub::run_tool({"/usr/bin/ditto", "-x", "-k", a, d});
#else
  // GNU tar reads .tar.gz everywhere; zip would need bsdtar. The Linux
  // payload is published as .tar.gz for exactly that reason.
  return stub::run_tool({"tar", targz ? "-xzf" : "-xf", a, "-C", d});
#endif
}

void prune(const Store &store, const std::string &keep1, const std::string &keep2) {
  std::error_code ec;
  for (auto &e : fs::directory_iterator(store.root, ec)) {
    if (!e.is_directory())
      continue;
    std::string name = e.path().filename().u8string();
    if (name == keep1 || name == keep2 || name == "staging")
      continue;
    std::error_code ec2;
    fs::remove_all(e.path(), ec2);
  }
}

// ---------------- the update ----------------
struct Fresh {
  bool applied = false;
  std::string id, prev;
};

Fresh try_update(stub::GuiState &st, const Store &store, const std::string &cur) {
  Fresh fresh;
  std::error_code ec;
  fs::remove_all(store.staging_dir(), ec); // stale staging from a killed run

  const std::string url = manifest_url();
  if (!url_allowed(url)) {
    stub::logf("manifest url not allowed: %s", url.c_str());
    return fresh;
  }
  std::string body;
  stub::HttpResult r = stub::http_get_text(url, 3000, &body);
  if (!r.ok) {
    stub::logf("manifest fetch failed: %s", r.error.c_str());
    return fresh;
  }
  const std::string build_id = stub::json_str_field(body, "build_id");
  const std::string dl_url = stub::json_str_field(body, std::string(kPlat) + "_url");
  const std::string sha = stub::json_str_field(body, std::string(kPlat) + "_sha256");
  if (!build_id.empty() && build_id == cur) {
    stub::logf("up to date (%s)", cur.c_str());
    return fresh;
  }
  if (build_id.empty() || dl_url.empty() || sha.empty()) {
    stub::logf("manifest offers nothing for %s", kPlat);
    return fresh;
  }
  if (!safe_id(build_id)) {
    stub::logf("manifest build_id rejected: %s", build_id.c_str());
    return fresh;
  }
  if (!url_allowed(dl_url)) {
    stub::logf("download url not allowed: %s", dl_url.c_str());
    return fresh;
  }

  stub::logf("updating %s -> %s", cur.empty() ? "(none)" : cur.c_str(),
             build_id.c_str());
  st.phase = stub::kDownloading;
  fs::create_directories(store.staging_dir(), ec);
  if (ec) {
    stub::logf("cannot create staging dir");
    return fresh;
  }
  auto cleanup = [&] {
    std::error_code ec2;
    fs::remove_all(store.staging_dir(), ec2);
  };
  const bool targz = ends_with(dl_url, ".tar.gz") || ends_with(dl_url, ".tgz");
  fs::path archive = store.staging_dir() / (targz ? "payload.tar.gz" : "payload.zip");
  r = stub::http_download(dl_url, S(archive), [&](uint64_t got, uint64_t total) {
    if (total)
      st.percent = (int)(got * 100 / total);
    return !st.cancel.load();
  });
  if (!r.ok) {
    stub::logf("download failed: %s", r.error.c_str());
    cleanup();
    return fresh;
  }
  const std::string got_sha = stub::sha256_file_hex(S(archive));
  if (got_sha != sha) {
    stub::logf("sha256 mismatch (got %s) - download discarded", got_sha.c_str());
    cleanup();
    return fresh;
  }
  fs::path x = store.staging_dir() / "x";
  if (!extract_archive(archive, x)) {
    stub::logf("extract failed");
    cleanup();
    return fresh;
  }
  if (!stub::verify_payload(S(x))) {
    stub::logf("payload signature check failed - update discarded");
    cleanup();
    return fresh;
  }
  if (!fs::exists(x / P(kPayloadRel))) {
    stub::logf("payload executable missing from the archive");
    cleanup();
    return fresh;
  }
  fs::path dest = store.version_dir(build_id);
  fs::remove_all(dest, ec);
  fs::rename(x, dest, ec);
  if (ec) {
    stub::logf("cannot move payload into place: %s", ec.message().c_str());
    cleanup();
    return fresh;
  }
  write_atomic(store.previous_file(), cur);
  if (!write_atomic(store.current_file(), build_id)) {
    stub::logf("cannot flip current.txt - keeping %s", cur.c_str());
    cleanup();
    return fresh;
  }
  cleanup();
  prune(store, build_id, cur);
  stub::logf("updated to %s", build_id.c_str());
  fresh.applied = true;
  fresh.id = build_id;
  fresh.prev = cur;
  return fresh;
}

// ---------------- launch ----------------
struct Launched {
  stub::Child child;
  bool ok = false;
};

Launched launch_version(const Store &store, const std::string &id) {
  Launched l;
  fs::path dir, exe;
  if (!id.empty()) {
    dir = store.version_dir(id);
    exe = dir / P(kPayloadRel);
  }
  if (id.empty() || !fs::exists(exe)) {
    // The seed payload shipped beside the stub (first run, or a wiped store).
#ifdef __APPLE__
    dir = exe_dir().parent_path() / "Resources/payload";
#else
    dir = exe_dir() / "payload";
#endif
    exe = dir / P(kPayloadRel);
  }
  if (!fs::exists(exe))
    return l;
  stub::logf("launching %s", S(exe).c_str());
  l.ok = stub::spawn(S(exe), S(dir), &l.child);
  return l;
}

} // namespace

void stub::logf(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  fprintf(stderr, "[stub] %s\n", buf);
  if (g_log.is_open()) {
    g_log << buf << "\n";
    g_log.flush();
  }
}

int stub_main() {
  stub::GuiState st;
  Store store = resolve_store();
  std::error_code ec;
  fs::create_directories(store.root, ec);
  log_open(store.root / "stub.log");
  stub::logf("start; store=%s", S(store.root).c_str());

  Fresh fresh;
  Launched launched;
  std::thread worker([&] {
    const std::string cur = read_line(store.current_file());
    fresh = try_update(st, store, cur);
    launched = launch_version(store, read_line(store.current_file()));
    if (!launched.ok && fresh.applied) {
      // The new payload can't even spawn - roll back immediately.
      stub::logf("rollback: spawn failed for %s", fresh.id.c_str());
      write_atomic(store.current_file(), fresh.prev);
      launched = launch_version(store, fresh.prev);
      fresh.applied = false;
    }
    if (launched.ok) {
      // Give the payload a beat to get its window up before ours closes.
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    st.done = true;
  });
  stub::gui_run(st);
  worker.join();

  if (!launched.ok) {
    stub::error_box("Silencer Launcher could not find its application files.\n"
                    "Please reinstall the launcher.");
    return 1;
  }
  if (fresh.applied) {
    // A freshly-updated payload that dies right away gets rolled back.
    int rc = stub::try_wait(launched.child, 5000);
    if (rc > 0) {
      stub::logf("rollback: %s exited with %d right after updating",
                 fresh.id.c_str(), rc);
      write_atomic(store.current_file(), fresh.prev);
      Launched again = launch_version(store, fresh.prev);
      if (!again.ok) {
        stub::error_box("Silencer Launcher could not start after an update.\n"
                        "Please reinstall the launcher.");
        return 1;
      }
    }
  }
  return 0;
}

#ifndef _WIN32
int main() { return stub_main(); }
#endif
