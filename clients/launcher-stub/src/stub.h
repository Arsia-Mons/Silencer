#pragma once

// Bootstrap stub (issue #347): the tiny always-works updater the user
// actually launches. It checks for a launcher update, applies it into a
// versioned store, and runs the current payload — falling back to the
// existing version on ANY failure. Its safety comes from having no
// dependencies and (almost) never changing; keep it that way.
//
// Every std::string crossing this interface is UTF-8. Platform files
// convert at the OS boundary.

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace stub {

// ---------- logging (main.cpp) ----------
void logf(const char *fmt, ...);

// ---------- GUI (per platform) ----------
// The worker thread drives these atomics; the platform GUI polls them.
enum Phase : int { kChecking = 0, kDownloading = 1 };
struct GuiState {
  std::atomic<int> phase{kChecking};
  std::atomic<int> percent{-1}; // -1 = indeterminate
  std::atomic<bool> done{false};   // worker -> GUI: close now
  std::atomic<bool> cancel{false}; // GUI -> worker: skip the update
};
// Shows the native window immediately and blocks until st.done.
// SILENCER_STUB_NO_GUI=1 (or no display on Linux) degrades to a plain wait.
void gui_run(GuiState &st);

// Native modal error box. Used only when there is nothing left to launch.
void error_box(const std::string &msg);

// ---------- HTTP (per platform) ----------
struct HttpResult {
  bool ok = false;
  std::string error;
};
// Small-body GET into memory. timeout_ms bounds each request stage.
HttpResult http_get_text(const std::string &url, int timeout_ms, std::string *out);
// Large download to a file. progress returning false aborts (cancel).
// No total timeout; a ~30s stall aborts.
using DlProgress = std::function<bool(uint64_t got, uint64_t total)>;
HttpResult http_download(const std::string &url, const std::string &dest,
                         const DlProgress &progress);

// ---------- processes (per platform) ----------
struct Child {
  intptr_t handle = 0; // HANDLE on Windows, pid elsewhere
  bool valid() const { return handle != 0; }
};
bool spawn(const std::string &exe, const std::string &workdir, Child *out);
// Exit code if the child exited within timeout_ms, else -1.
int try_wait(const Child &c, int timeout_ms);
// Run a helper to completion, no visible window. argv[0] is the program.
bool run_tool(const std::vector<std::string> &argv);

// ---------- platform glue ----------
std::string env_str(const char *name);
std::string own_exe_path();
// Signature check on the staged payload. Windows/Linux: sha256 already
// verified, returns true. macOS: codesign Developer ID check against the
// running stub's own team id; an unsigned stub (dev, e2e) skips it.
bool verify_payload(const std::string &extracted_dir);

// ---------- portable helpers ----------
// First string value for "key" in a small flat JSON object ("" if absent).
// Not a JSON parser — good enough for our own manifests, and any confusion
// just means "no update", which is safe.
std::string json_str_field(const std::string &json, const std::string &key);
std::string sha256_file_hex(const std::string &path);

} // namespace stub

// Portable entry point; called from main() / wWinMain().
int stub_main();
