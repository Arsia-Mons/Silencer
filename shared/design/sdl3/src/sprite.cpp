#include "sprite.h"

#include <array>
#include <cstdio>
#include <cstring>

namespace silencer {

namespace {

// Decode RLE pixel stream into out, stopping when out has reached
// out_capacity bytes. Returns the number of source bytes consumed.
// The stream is little-endian dwords.
size_t DecodeRleLinear(const uint8_t *src, size_t src_len,
                       uint8_t *out, size_t out_capacity) {
    size_t si = 0;
    size_t oi = 0;
    while (oi < out_capacity && si + 4 <= src_len) {
        uint32_t dword = static_cast<uint32_t>(src[si]) |
                         (static_cast<uint32_t>(src[si + 1]) << 8) |
                         (static_cast<uint32_t>(src[si + 2]) << 16) |
                         (static_cast<uint32_t>(src[si + 3]) << 24);
        si += 4;
        if ((dword & 0xFF000000u) == 0xFF000000u) {
            // run marker
            uint32_t run_bytes = dword & 0x0000FFFFu;
            uint8_t pixel = static_cast<uint8_t>((dword >> 16) & 0xFFu);
            for (uint32_t k = 0; k < run_bytes && oi < out_capacity; ++k) {
                out[oi++] = pixel;
            }
        } else {
            // literal: 4 raw bytes
            uint8_t b0 = static_cast<uint8_t>(dword & 0xFFu);
            uint8_t b1 = static_cast<uint8_t>((dword >> 8) & 0xFFu);
            uint8_t b2 = static_cast<uint8_t>((dword >> 16) & 0xFFu);
            uint8_t b3 = static_cast<uint8_t>((dword >> 24) & 0xFFu);
            if (oi < out_capacity) out[oi++] = b0;
            if (oi < out_capacity) out[oi++] = b1;
            if (oi < out_capacity) out[oi++] = b2;
            if (oi < out_capacity) out[oi++] = b3;
        }
    }
    return si;
}

// Re-arrange a linear emission buffer into the sprite's w*h pixel grid
// according to the tile-mode rule described in sprite-banks.md:
// outer over tile rows, then tile cols, inner over rows in tile, then
// 4-pixel-wide chunks across the tile. Tile size = 64x64. Edge tiles
// truncate to remaining w/h.
void ScatterTileMode(const std::vector<uint8_t> &linear,
                     uint8_t *out, int w, int h) {
    const int kTile = 64;
    int tiles_y = (h + kTile - 1) / kTile;
    int tiles_x = (w + kTile - 1) / kTile;
    size_t li = 0;
    for (int ty = 0; ty < tiles_y; ++ty) {
        int row0 = ty * kTile;
        int row_end = std::min(h, row0 + kTile);
        int rows = row_end - row0;
        for (int tx = 0; tx < tiles_x; ++tx) {
            int col0 = tx * kTile;
            int col_end = std::min(w, col0 + kTile);
            int cols = col_end - col0;
            // Within tile: row-major, but consume 4 pixels at a time
            for (int rr = 0; rr < rows; ++rr) {
                int y = row0 + rr;
                int x = col0;
                int remaining = cols;
                while (remaining > 0) {
                    int chunk = remaining >= 4 ? 4 : remaining;
                    for (int k = 0; k < chunk; ++k) {
                        if (li < linear.size()) {
                            out[y * w + (x + k)] = linear[li++];
                        }
                    }
                    // Even on partial chunks, the source emitted 4 bytes
                    // per literal — but our linear buffer was already
                    // capped at w*h on decode, so we just move on.
                    x += chunk;
                    remaining -= chunk;
                }
            }
        }
    }
}

}  // namespace

bool Sprites::LoadIndex(const std::string &assets_dir) {
    std::string path = assets_dir + "/BIN_SPR.DAT";
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "BIN_SPR.DAT: open failed: %s\n", path.c_str());
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(sz);
    size_t got = std::fread(buf.data(), 1, sz, f);
    std::fclose(f);
    if (got != static_cast<size_t>(sz)) return false;

    // SPEC GAP: docs/design/sprite-banks.md says BIN_SPR.DAT is 256
    // bytes, one byte per bank. The actual file is 16384 bytes
    // (256 banks * 64-byte records). Inspecting the bytes shows the
    // sprite count for bank N lives at byte offset 2 of record N
    // (e.g. bank 6 -> 0x21 = 33 sprites, bank 208 -> 0x3d = 61).
    // We honor the file as it exists.
    if (sz == 256) {
        for (int i = 0; i < 256; ++i) counts_[i] = buf[i];
    } else if (sz == 16384) {
        for (int i = 0; i < 256; ++i) counts_[i] = buf[i * 64 + 2];
    } else {
        std::fprintf(stderr, "BIN_SPR.DAT: unexpected size %ld\n", sz);
        return false;
    }
    return true;
}

bool Sprites::LoadBank(const std::string &assets_dir, int bank) {
    if (banks_[bank].loaded) return true;
    int count = counts_[bank];
    if (count == 0) {
        std::fprintf(stderr, "bank %d: count is 0, no sprites\n", bank);
        return false;
    }

    char fname[256];
    std::snprintf(fname, sizeof(fname), "%s/bin_spr/SPR_%03d.BIN",
                  assets_dir.c_str(), bank);
    FILE *f = std::fopen(fname, "rb");
    if (!f) {
        std::fprintf(stderr, "bank %d: open failed: %s\n", bank, fname);
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(sz);
    if (std::fread(buf.data(), 1, sz, f) != static_cast<size_t>(sz)) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    SpriteBank &b = banks_[bank];
    b.bank = bank;
    b.count = count;
    b.sprites.resize(count);

    const size_t hdr_section = static_cast<size_t>(344) * count + 4;
    if (buf.size() < hdr_section) {
        std::fprintf(stderr, "bank %d: file too small for headers\n", bank);
        return false;
    }

    // Parse headers
    for (int i = 0; i < count; ++i) {
        const uint8_t *h = buf.data() + i * 344;
        Sprite &s = b.sprites[i];
        s.w = static_cast<uint16_t>(h[0] | (h[1] << 8));
        s.h = static_cast<uint16_t>(h[2] | (h[3] << 8));
        s.offset_x = static_cast<int16_t>(h[4] | (h[5] << 8));
        s.offset_y = static_cast<int16_t>(h[6] | (h[7] << 8));
        s.comp_size = static_cast<uint32_t>(h[12]) |
                      (static_cast<uint32_t>(h[13]) << 8) |
                      (static_cast<uint32_t>(h[14]) << 16) |
                      (static_cast<uint32_t>(h[15]) << 24);
        s.mode = h[20];
    }

    // Decode pixel data: starts at hdr_section.
    // Per spec, comp_size is unreliable for tile mode, so we
    // pre-pass with output-size termination and use the consumed
    // byte count to advance.
    size_t cursor = hdr_section;
    for (int i = 0; i < count; ++i) {
        Sprite &s = b.sprites[i];
        size_t out_cap = static_cast<size_t>(s.w) * s.h;
        std::vector<uint8_t> linear(out_cap, 0);
        if (cursor > buf.size()) break;
        size_t consumed = DecodeRleLinear(
            buf.data() + cursor, buf.size() - cursor,
            linear.data(), out_cap);
        cursor += consumed;

        s.pixels.assign(out_cap, 0);
        if (s.mode == 0) {
            std::memcpy(s.pixels.data(), linear.data(), out_cap);
        } else {
            ScatterTileMode(linear, s.pixels.data(), s.w, s.h);
        }
    }

    b.loaded = true;
    return true;
}

void Sprites::Blit(uint8_t *fb, int fb_w, int fb_h,
                   const Sprite &spr, int top_left_x, int top_left_y,
                   const uint8_t *tint_lut) {
    int w = spr.w;
    int h = spr.h;
    for (int sy = 0; sy < h; ++sy) {
        int dy = top_left_y + sy;
        if (dy < 0 || dy >= fb_h) continue;
        for (int sx = 0; sx < w; ++sx) {
            int dx = top_left_x + sx;
            if (dx < 0 || dx >= fb_w) continue;
            uint8_t p = spr.pixels[sy * w + sx];
            if (p == 0) continue;  // transparent
            uint8_t out = tint_lut ? tint_lut[p] : p;
            fb[dy * fb_w + dx] = out;
        }
    }
}

}  // namespace silencer
