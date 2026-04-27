#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace silencer {

struct Sprite {
  int w = 0;
  int h = 0;
  int offset_x = 0;
  int offset_y = 0;
  uint32_t comp_size = 0;
  uint8_t mode = 0;
  std::vector<uint8_t> pixels;  // size = w*h, palette indices, 0 = transparent
};

struct SpriteBank {
  int bank_id = -1;
  std::vector<Sprite> sprites;
};

// Indexed by bank number 0..255. Banks not loaded have empty sprites.
class SpriteSet {
 public:
  // Reads `assets_dir/BIN_SPR.DAT` to obtain per-bank sprite counts, then
  // loads each requested bank from `assets_dir/bin_spr/SPR_NNN.BIN`.
  bool Load(const std::string &assets_dir, const std::vector<int> &banks);

  const Sprite &Get(int bank, int idx) const { return banks_[bank].sprites[idx]; }
  bool Has(int bank, int idx) const {
    if (bank < 0 || bank >= 256) return false;
    return idx >= 0 && idx < static_cast<int>(banks_[bank].sprites.size());
  }

 private:
  std::array<SpriteBank, 256> banks_{};
  std::array<uint8_t, 256> sprite_counts_{};

  bool LoadIndex(const std::string &path);
  bool LoadBank(int bank, const std::string &path);
};

// Indexed framebuffer 640x480.
struct Framebuffer {
  static constexpr int W = 640;
  static constexpr int H = 480;
  std::array<uint8_t, W * H> px{};

  void Clear() { px.fill(0); }
};

// Blit a sprite to the framebuffer using the anchor-offset convention:
//   top_left = (object_x - sprite.offset_x, object_y - sprite.offset_y)
// Index 0 in source pixels is transparent. `tint_lut` may be nullptr for
// identity mapping.
void BlitSprite(Framebuffer &fb, const Sprite &s, int object_x, int object_y,
                const uint8_t *tint_lut);

}  // namespace silencer
