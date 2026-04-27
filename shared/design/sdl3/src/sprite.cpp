#include "sprite.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace silencer {

namespace {

uint16_t ReadU16LE(const uint8_t *p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
int16_t ReadI16LE(const uint8_t *p) {
  return static_cast<int16_t>(ReadU16LE(p));
}
uint32_t ReadU32LE(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// Decode one sprite's RLE pixel stream into a linear (row-major) buffer of
// size w*h. Returns the number of source bytes consumed. The `mode` byte
// selects linear vs. tile arrangement.
size_t DecodeSprite(const uint8_t *src, size_t src_size, int w, int h,
                    uint8_t mode, std::vector<uint8_t> *out) {
  out->assign(static_cast<size_t>(w) * h, 0);

  // Helper: emit byte to the output stream. Sequential indices in the
  // raw decode order; we'll re-arrange at the end if mode != 0.
  std::vector<uint8_t> raw;
  raw.reserve(static_cast<size_t>(w) * h);

  size_t pos = 0;
  size_t target = static_cast<size_t>(w) * h;
  while (raw.size() < target && pos + 4 <= src_size) {
    uint32_t dword = ReadU32LE(src + pos);
    pos += 4;
    if ((dword & 0xFF000000u) == 0xFF000000u) {
      uint32_t run_bytes = dword & 0x0000FFFFu;
      uint8_t pixel = static_cast<uint8_t>((dword >> 16) & 0xFFu);
      for (uint32_t k = 0; k < run_bytes && raw.size() < target; ++k) {
        raw.push_back(pixel);
      }
    } else {
      // 4 raw pixels (little endian byte order: byte0..byte3).
      uint8_t b0 = static_cast<uint8_t>(dword & 0xFFu);
      uint8_t b1 = static_cast<uint8_t>((dword >> 8) & 0xFFu);
      uint8_t b2 = static_cast<uint8_t>((dword >> 16) & 0xFFu);
      uint8_t b3 = static_cast<uint8_t>((dword >> 24) & 0xFFu);
      if (raw.size() < target) raw.push_back(b0);
      if (raw.size() < target) raw.push_back(b1);
      if (raw.size() < target) raw.push_back(b2);
      if (raw.size() < target) raw.push_back(b3);
    }
  }

  if (mode == 0) {
    // Linear: row-major.
    std::memcpy(out->data(), raw.data(),
                std::min(raw.size(), static_cast<size_t>(w) * h));
  } else {
    // Tile mode: 64x64 tiles. Outer iteration over tile rows, then tile
    // columns. Inner iteration: for each row within the tile, emit pixels
    // in 4-pixel-wide chunks across the tile.
    constexpr int TS = 64;
    size_t ri = 0;
    int n_tile_rows = (h + TS - 1) / TS;
    int n_tile_cols = (w + TS - 1) / TS;
    for (int tr = 0; tr < n_tile_rows; ++tr) {
      int tile_h = std::min(TS, h - tr * TS);
      for (int tc = 0; tc < n_tile_cols; ++tc) {
        int tile_w = std::min(TS, w - tc * TS);
        for (int ty = 0; ty < tile_h; ++ty) {
          // Inside one row of one tile: 4-pixel chunks across the tile_w.
          int x = 0;
          while (x < tile_w) {
            int chunk = std::min(4, tile_w - x);
            for (int k = 0; k < chunk; ++k) {
              if (ri < raw.size()) {
                int dst_x = tc * TS + x + k;
                int dst_y = tr * TS + ty;
                if (dst_x < w && dst_y < h) {
                  (*out)[dst_y * w + dst_x] = raw[ri];
                }
                ++ri;
              }
            }
            x += chunk;
          }
        }
      }
    }
  }

  return pos;
}

}  // namespace

bool SpriteSet::LoadIndex(const std::string &path) {
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) {
    std::fprintf(stderr, "sprite: cannot open %s\n", path.c_str());
    return false;
  }
  std::vector<uint8_t> buf(16384, 0);
  size_t n = std::fread(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  if (n < 16384) {
    std::fprintf(stderr, "sprite: short read on %s (%zu bytes)\n", path.c_str(), n);
    return false;
  }
  for (int i = 0; i < 256; ++i) {
    sprite_counts_[i] = buf[i * 64 + 2];
  }
  return true;
}

bool SpriteSet::LoadBank(int bank, const std::string &path) {
  int count = sprite_counts_[bank];
  if (count == 0) {
    std::fprintf(stderr, "sprite: bank %d has count=0, skipping\n", bank);
    return false;
  }

  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) {
    std::fprintf(stderr, "sprite: cannot open %s\n", path.c_str());
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  long file_size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> all(file_size);
  std::fread(all.data(), 1, file_size, f);
  std::fclose(f);

  // Header block: 344 * count + 4 bytes of trailing padding.
  size_t header_block = static_cast<size_t>(344) * count + 4;
  if (header_block > all.size()) {
    std::fprintf(stderr, "sprite: bank %d too small for headers\n", bank);
    return false;
  }

  banks_[bank].bank_id = bank;
  banks_[bank].sprites.resize(count);

  // Pre-decode each sprite. We use the "output-size termination" strategy
  // and advance the pixel cursor by however many bytes DecodeSprite
  // consumed.
  size_t pixel_cursor = header_block;
  for (int i = 0; i < count; ++i) {
    const uint8_t *h = all.data() + static_cast<size_t>(344) * i;
    Sprite &s = banks_[bank].sprites[i];
    s.w = ReadU16LE(h + 0);
    s.h = ReadU16LE(h + 2);
    s.offset_x = ReadI16LE(h + 4);
    s.offset_y = ReadI16LE(h + 6);
    s.comp_size = ReadU32LE(h + 12);
    s.mode = h[20];

    if (s.w <= 0 || s.h <= 0) {
      // Empty sprite; nothing to consume.
      continue;
    }

    if (pixel_cursor >= all.size()) {
      std::fprintf(stderr, "sprite: bank %d sprite %d ran past EOF\n", bank, i);
      break;
    }

    size_t consumed = DecodeSprite(all.data() + pixel_cursor,
                                   all.size() - pixel_cursor, s.w, s.h,
                                   s.mode, &s.pixels);
    pixel_cursor += consumed;
  }

  return true;
}

bool SpriteSet::Load(const std::string &assets_dir,
                     const std::vector<int> &banks) {
  std::string idx = assets_dir + "/BIN_SPR.DAT";
  if (!LoadIndex(idx)) return false;

  for (int b : banks) {
    char suffix[32];
    std::snprintf(suffix, sizeof(suffix), "/bin_spr/SPR_%03d.BIN", b);
    std::string fname = assets_dir + suffix;
    if (!LoadBank(b, fname)) {
      std::fprintf(stderr, "sprite: failed loading bank %d (%s)\n", b,
                   fname.c_str());
      // Continue — font banks 134/136 are not needed by the menu.
    }
  }
  return true;
}

void BlitSprite(Framebuffer &fb, const Sprite &s, int object_x, int object_y,
                const uint8_t *tint_lut) {
  if (s.w <= 0 || s.h <= 0) return;
  int top_left_x = object_x - s.offset_x;
  int top_left_y = object_y - s.offset_y;

  for (int sy = 0; sy < s.h; ++sy) {
    int dy = top_left_y + sy;
    if (dy < 0 || dy >= Framebuffer::H) continue;
    for (int sx = 0; sx < s.w; ++sx) {
      int dx = top_left_x + sx;
      if (dx < 0 || dx >= Framebuffer::W) continue;
      uint8_t p = s.pixels[sy * s.w + sx];
      if (p == 0) continue;
      uint8_t out = tint_lut ? tint_lut[p] : p;
      fb.px[dy * Framebuffer::W + dx] = out;
    }
  }
}

}  // namespace silencer
