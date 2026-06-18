#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace client::ui {

// The per-map minimap previews surfaced to .cppx screens. The renderer bridge
// bakes each bundled map's 172x62 indexed minimap into premultiplied RGBA,
// keyed by filename; screens look it up via texture_for(filename).
//
// An id of 0 means "no preview": screens MUST tolerate it (render nothing).
struct MapPreviews {
  static constexpr int kWidth = 172;
  static constexpr int kHeight = 62;

  std::unordered_map<std::string, uint32_t> by_filename = {};
  // The map's name + description (from the header), shown under the minimap.
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

// Read the baked map-preview textures for the current frame; empty if no provider.
MapPreviews use_map_previews();

} // namespace client::ui
