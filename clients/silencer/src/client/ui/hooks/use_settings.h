#pragma once

#include <cstdint>
#include <functional>

namespace client::ui {

// Persisted user prefs (doc §6 use_settings). Mirrors the legacy
// OptionsAudio/OptionsDisplay screens 1:1 — the only prefs those screens
// edited. Read fields are the current (live, possibly-uncommitted) values;
// set_* live-apply a preview immediately (SIL-6 LOCKED: live-apply preview);
// commit() persists to disk; revert() restores the last-saved values and
// re-applies them. `dirty` is true when live state diverges from disk.
// App-shell, not game-coupled (never names Game) — the composition root
// installs the routing closures over the public Audio/GameRenderer/window
// seams.
struct Settings {
  // ---- audio ----
  bool music = true;          // Config::music
  uint8_t music_volume = 48;  // Config::musicvolume (0..128)
  // ---- display ----
  bool fullscreen = true;     // Config::fullscreen
  bool smooth_scaling = true; // Config::scalefilter

  bool dirty = false; // live state != on-disk state

  std::function<void(bool)> set_music = {};
  std::function<void(uint8_t)> set_music_volume = {};
  std::function<void(bool)> set_fullscreen = {};
  std::function<void(bool)> set_smooth_scaling = {};

  std::function<void()> commit = {}; // persist (Config::Save)
  std::function<void()> revert = {}; // reload last-saved + re-apply
};

Settings use_settings();

} // namespace client::ui
