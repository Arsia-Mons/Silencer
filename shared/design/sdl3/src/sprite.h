// Sprite-bank loader (BIN_SPR.DAT + bin_spr/SPR_NNN.BIN), RLE codec, and
// 8-bit indexed blitter that respects palette index 0 as transparent.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace silencer {

class Palette;

struct Sprite {
    std::uint16_t w{0};
    std::uint16_t h{0};
    std::int16_t offset_x{0};
    std::int16_t offset_y{0};
    std::vector<std::uint8_t> pixels;  // size = w*h, palette-indexed.
};

class SpriteBanks {
   public:
    static constexpr std::size_t kBankCount = 256;

    // dir = path to the assets root (the directory containing BIN_SPR.DAT and bin_spr/).
    bool LoadIndex(const std::string& assets_dir);

    // Loads a single bank from bin_spr/SPR_NNN.BIN. Safe to call multiple times.
    bool LoadBank(unsigned bank);

    bool Has(unsigned bank, unsigned index) const {
        if (bank >= kBankCount) return false;
        const auto& v = banks_[bank];
        return index < v.size() && v[index] && v[index]->w > 0;
    }

    const Sprite* Get(unsigned bank, unsigned index) const {
        if (bank >= kBankCount) return nullptr;
        const auto& v = banks_[bank];
        if (index >= v.size()) return nullptr;
        return v[index].get();
    }

    std::uint8_t SpriteCount(unsigned bank) const {
        return bank < kBankCount ? sprite_counts_[bank] : 0;
    }

    // Blit `bank/index` onto an 8-bit indexed surface of size dst_w x dst_h.
    // Index 0 in the source is transparent. (x, y) is the *anchor* point — the
    // sprite is placed at (x - offset_x, y - offset_y) per §Shared Base.
    // If `tint_lookup` is non-null it remaps every non-zero source index
    // through the table before writing (used for EffectColor / EffectBrightness
    // tinting at the index level — callers can build the lookup once).
    void Blit(std::uint8_t* dst, int dst_w, int dst_h,
              unsigned bank, unsigned index,
              int x, int y,
              const std::uint8_t* tint_lookup = nullptr,
              bool mirrored = false) const;

   private:
    std::vector<std::unique_ptr<Sprite>> banks_[kBankCount];
    std::uint8_t sprite_counts_[kBankCount]{};
    std::string assets_dir_;
    bool index_loaded_{false};

    // Returns the number of source bytes consumed (some sprite headers
    // overstate `comp_size` for tile-mode sprites; the actual stride is
    // however many dwords were needed to fill w*h pixels).
    std::size_t DecodeRle(const std::uint8_t* src, std::size_t src_size, std::uint8_t mode,
                          std::uint16_t w, std::uint16_t h, std::vector<std::uint8_t>& out) const;
};

}  // namespace silencer
