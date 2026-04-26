#include "sprite.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace silencer {

namespace {

inline std::uint16_t ReadU16LE(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
inline std::int16_t ReadI16LE(const std::uint8_t* p) {
    return static_cast<std::int16_t>(p[0] | (p[1] << 8));
}
inline std::uint32_t ReadU32LE(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

bool SpriteBanks::LoadIndex(const std::string& assets_dir) {
    assets_dir_ = assets_dir;
    if (!assets_dir_.empty() && assets_dir_.back() != '/') assets_dir_ += '/';

    std::FILE* f = std::fopen((assets_dir_ + "BIN_SPR.DAT").c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "SpriteBanks: cannot open %sBIN_SPR.DAT\n", assets_dir_.c_str());
        return false;
    }
    std::uint8_t buf[16384];
    std::size_t got = std::fread(buf, 1, sizeof(buf), f);
    std::fclose(f);
    if (got != sizeof(buf)) {
        std::fprintf(stderr, "SpriteBanks: BIN_SPR.DAT short read (%zu)\n", got);
        return false;
    }
    for (std::size_t b = 0; b < kBankCount; ++b) {
        sprite_counts_[b] = buf[b * 64 + 2];
    }
    index_loaded_ = true;
    return true;
}

std::size_t SpriteBanks::DecodeRle(const std::uint8_t* src, std::size_t src_size,
                                   std::uint8_t mode, std::uint16_t w, std::uint16_t h,
                                   std::vector<std::uint8_t>& out) const {
    const std::size_t total_pixels = static_cast<std::size_t>(w) * h;
    if (total_pixels == 0) {
        out.clear();
        return 0;
    }

    // Decompress the dword stream into a linear byte buffer of exactly
    // total_pixels bytes. The real client reads dwords inline during tile
    // traversal until all pixels are filled; we materialize that as a
    // pre-pass so the spatial redistribution below can remain simple.
    //
    // Tile-mode sprites' `comp_size` overstates the consumed stride in some
    // banks (e.g. bank 6 idx 0 — the 640x480 menu plate). Treat src_size as
    // an upper bound only; stop as soon as we have enough decoded pixels.
    std::vector<std::uint8_t> linear;
    linear.reserve(total_pixels);

    std::size_t pos = 0;
    while (pos + 4 <= src_size && linear.size() < total_pixels) {
        std::uint32_t d = ReadU32LE(src + pos);
        pos += 4;
        if ((d & 0xFF000000u) == 0xFF000000u) {
            std::uint32_t run_bytes = d & 0x0000FFFFu;  // always a multiple of 4
            std::uint8_t pixel = static_cast<std::uint8_t>((d >> 16) & 0xFFu);
            for (std::uint32_t i = 0; i < run_bytes; ++i) {
                linear.push_back(pixel);
            }
        } else {
            linear.push_back(static_cast<std::uint8_t>(d & 0xFFu));
            linear.push_back(static_cast<std::uint8_t>((d >> 8) & 0xFFu));
            linear.push_back(static_cast<std::uint8_t>((d >> 16) & 0xFFu));
            linear.push_back(static_cast<std::uint8_t>((d >> 24) & 0xFFu));
        }
    }
    if (linear.size() < total_pixels) linear.resize(total_pixels, 0);
    if (linear.size() > total_pixels) linear.resize(total_pixels);

    out.assign(total_pixels, 0);

    if (mode == 0) {
        // Linear: byte stream is already row-major.
        std::memcpy(out.data(), linear.data(), total_pixels);
        return pos;
    }

    // Tile-ordered: 64x64 tiles, row-major over the tile grid; each tile is
    // also row-major within its own pixels; partial edge tiles are clipped.
    constexpr int kTile = 64;
    std::size_t lin_pos = 0;
    for (int ty = 0; ty < h; ty += kTile) {
        int th = std::min(kTile, h - ty);
        for (int tx = 0; tx < w; tx += kTile) {
            int tw = std::min(kTile, w - tx);
            for (int row = 0; row < th; ++row) {
                if (lin_pos + tw > linear.size()) return pos;
                std::memcpy(out.data() + (ty + row) * w + tx,
                            linear.data() + lin_pos, tw);
                lin_pos += tw;
            }
        }
    }
    return pos;
}

bool SpriteBanks::LoadBank(unsigned bank) {
    if (!index_loaded_ || bank >= kBankCount) return false;
    if (!banks_[bank].empty()) return true;
    std::uint8_t count = sprite_counts_[bank];
    if (count == 0) return true;  // bank unused

    char name[32];
    std::snprintf(name, sizeof(name), "bin_spr/SPR_%03u.BIN", bank);
    std::string path = assets_dir_ + name;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "SpriteBanks: cannot open %s\n", path.c_str());
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> data(size);
    std::fread(data.data(), 1, size, f);
    std::fclose(f);

    constexpr std::size_t kHeader = 344;
    // Real client reads `(344 * count) + 4` bytes of header data before any
    // pixel data — the trailing 4 bytes are filler we skip but they shift
    // pixel offsets by 4. See clients/silencer/src/resources.cpp:56.
    std::size_t pixel_off = static_cast<std::size_t>(count) * kHeader + 4;
    banks_[bank].resize(count);
    for (unsigned i = 0; i < count; ++i) {
        const std::uint8_t* h = data.data() + i * kHeader;
        auto sp = std::make_unique<Sprite>();
        sp->w = ReadU16LE(h + 0);
        sp->h = ReadU16LE(h + 2);
        sp->offset_x = ReadI16LE(h + 4);
        sp->offset_y = ReadI16LE(h + 6);
        std::uint32_t comp_size = ReadU32LE(h + 12);
        std::uint8_t mode = h[20];
        // Tile-mode sprites' `comp_size` overstates the actual byte stride
        // in some banks. DecodeRle returns the actual bytes consumed; use
        // that to advance to the next sprite. For mode 0, consumed == comp_size.
        std::size_t available = data.size() > pixel_off ? data.size() - pixel_off : 0;
        std::size_t budget = std::min<std::size_t>(comp_size, available);
        if (mode != 0) budget = available;  // tile mode: scan to fill output
        std::size_t consumed = 0;
        if (sp->w > 0 && sp->h > 0 && budget > 0) {
            consumed = DecodeRle(data.data() + pixel_off, budget, mode, sp->w, sp->h, sp->pixels);
        }
        pixel_off += (mode == 0) ? comp_size : consumed;
        banks_[bank][i] = std::move(sp);
    }
    return true;
}

void SpriteBanks::Blit(std::uint8_t* dst, int dst_w, int dst_h, unsigned bank,
                       unsigned index, int x, int y,
                       const std::uint8_t* tint_lookup, bool mirrored) const {
    const Sprite* sp = Get(bank, index);
    if (!sp || sp->pixels.empty()) return;

    int x0 = x - sp->offset_x;
    int y0 = y - sp->offset_y;
    for (int sy = 0; sy < sp->h; ++sy) {
        int dy = y0 + sy;
        if (dy < 0 || dy >= dst_h) continue;
        for (int sx = 0; sx < sp->w; ++sx) {
            int dx = x0 + (mirrored ? (sp->w - 1 - sx) : sx);
            if (dx < 0 || dx >= dst_w) continue;
            std::uint8_t p = sp->pixels[sy * sp->w + sx];
            if (p == 0) continue;  // transparent
            std::uint8_t out = tint_lookup ? tint_lookup[p] : p;
            dst[dy * dst_w + dx] = out;
        }
    }
}

}  // namespace silencer
