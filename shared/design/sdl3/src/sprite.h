#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace silencer {

struct Sprite {
    uint16_t w = 0;
    uint16_t h = 0;
    int16_t offset_x = 0;
    int16_t offset_y = 0;
    uint32_t comp_size = 0;
    uint8_t mode = 0;
    std::vector<uint8_t> pixels;  // w*h indexed bytes (0 = transparent)
};

struct SpriteBank {
    int bank = -1;
    int count = 0;
    std::vector<Sprite> sprites;
    bool loaded = false;
};

class Sprites {
public:
    // Loads BIN_SPR.DAT (count table) into per-bank counts.
    // assets_dir should not have a trailing slash.
    bool LoadIndex(const std::string &assets_dir);

    // Loads bank N from bin_spr/SPR_NNN.BIN. No-op if already loaded.
    bool LoadBank(const std::string &assets_dir, int bank);

    int Count(int bank) const { return counts_[bank]; }
    const SpriteBank &Bank(int bank) const { return banks_[bank]; }
    bool BankLoaded(int bank) const { return banks_[bank].loaded; }

    // Blit a sprite into an indexed framebuffer of size fb_w*fb_h.
    // top_left_(x,y) = object_(x,y) - sprite.offset_(x,y).
    // tint_lut is a 256-byte mapping; pass nullptr for identity.
    static void Blit(uint8_t *fb, int fb_w, int fb_h,
                     const Sprite &spr, int top_left_x, int top_left_y,
                     const uint8_t *tint_lut);

private:
    std::array<uint8_t, 256> counts_{};
    std::array<SpriteBank, 256> banks_{};
};

}  // namespace silencer
