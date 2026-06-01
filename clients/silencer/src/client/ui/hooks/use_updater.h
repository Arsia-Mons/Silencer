#pragma once

#include <functional>
#include <string>

namespace client::ui {

// UI-facing mirror of the self-updater state machine (src/updater). Decoupled
// from the ::Updater class so the UI never names it; the composition root maps
// ::Updater::State -> UpdaterPhase.
enum class UpdaterPhase {
  Idle,
  Prompting,
  Downloading,
  Verifying,
  Staging,
  Failed,
  Done,
};

// The updater model (doc §6): read state + the four user intents. Intents route
// to the public ::Updater methods via composition-root closures. Polling the
// updater's atomics (GetState/GetProgress/...) is main-thread-safe.
struct UpdaterModel {
  UpdaterPhase phase = UpdaterPhase::Idle;
  float progress = 0.0f;
  std::string error = {};
  int retry_count = 0;
  std::string download_url = {};
  bool can_retry = false;

  std::function<void()> consent = {};
  std::function<void()> cancel = {};
  std::function<void()> retry = {};
  std::function<void()> open_download_page = {};
};

UpdaterModel use_updater();

} // namespace client::ui
