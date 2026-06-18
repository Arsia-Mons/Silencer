#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace client::ui {

// The per-map minimap previews surfaced to .cppx screens. The renderer
// bridge bakes each bundled map's stored 172x62 indexed minimap + the active
// palette into a premultiplied-RGBA texture once (keyed by map filename) and
// hands the opaque `texture_id`s here, mirroring how use_chrome() surfaces baked
// chrome sprites. The Create-Game map list shows the hovered map's preview by
// looking its texture up via texture_for(filename); screens NEVER see SDL,
// Surface, or the Palette.
//
// An id of 0 means "no preview" (not baked yet, or the map header/minimap failed
// to load): screens MUST tolerate it (render nothing) without a crash or flash.
struct MapPreviews {
  // The stored minimap dimensions (fixed by the map format).
  static constexpr int kWidth = 172;
  static constexpr int kHeight = 62;

  std::unordered_map<std::string, uint32_t> by_filename = {};
  // The hovered map's name + description (from the map header) so the
  // cursor-following preview card shows them under the minimap, like origin.
  std::unordered_map<std::string, std::string> name_by_filename = {};
  std::unordered_map<std::string, std::string> desc_by_filename = {};

  uint32_t texture_for(const std::string &filename) const {
    auto it = by_filename.find(filename);
    return it == by_filename.end() ? 0 : it->second;
  }
  std::string name_for(const std::string &filename) const {
    auto it = name_by_filename.find(filename);
    return it == name_by_filename.end() ? std::string() : it->second;
  }
  std::string description_for(const std::string &filename) const {
    auto it = desc_by_filename.find(filename);
    return it == desc_by_filename.end() ? std::string() : it->second;
  }
};

// Read the baked map-preview textures for the current frame. Requires a
// MapPreviewsProvider above the caller (the composition root installs it);
// returns a default (empty) table if absent.
MapPreviews use_map_previews();

} // namespace client::ui
