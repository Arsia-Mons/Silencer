#pragma once

#include <cstdint>
#include <functional>

namespace client::ui {

// Persisted user prefs. Read fields are the live (possibly-uncommitted) values;
// set_* live-apply a preview immediately; commit() persists; revert() restores
// + re-applies the last-saved values. `dirty` = live state diverges from disk.
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
