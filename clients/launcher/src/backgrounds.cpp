#include "backgrounds.h"

#include "render/cppx_ui/glyph_fonts.h"
#include "render/cppx_ui/pipeline_host.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

namespace launcher {

namespace {

// Decoding mirrors Resources::LoadSprites (clients/silencer/src/resources/
// resources.cpp) and the palette load in src/render/palette.cpp — the same
// logic as tools/export_sprite_backgrounds.ts, which validated it against
// every bank.

std::vector<uint8_t> read_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return {};
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
}

uint32_t rd_u32(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, 4);
  return v; // little-endian targets only, same assumption as the game
}

uint16_t rd_u16(const uint8_t *p) {
  uint16_t v;
  memcpy(&v, p, 2);
  return v;
}

void wr_u32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

struct Sprite {
  int w = 0;
  int h = 0;
  std::vector<uint8_t> px;
};

// RLE marker: >= 0xff000000 => low 16 bits are the run length in bytes and
// bits 16-23 carry the palette index, replicated across the u32.
uint32_t rle_expand(uint32_t v) {
  v &= 0x00ff0000u;
  v |= v << 8;
  v |= v >> 16;
  return v;
}

std::vector<Sprite> decode_bank(const std::string &assets_dir,
                                const std::vector<uint8_t> &counts, int bank) {
  const int n = counts.size() > (size_t)(bank * 64 + 2) ? counts[bank * 64 + 2] : 0;
  if (!n)
    return {};
  char name[32];
  snprintf(name, sizeof(name), "/bin_spr/SPR_%03d.BIN", bank);
  const std::vector<uint8_t> buf = read_file(assets_dir + name);
  if (buf.empty())
    return {};

  size_t cursor = (size_t)344 * n + 4; // pixel data follows the header block
  std::vector<Sprite> out;
  out.reserve(n);

  for (int j = 0; j < n; ++j) {
    const uint8_t *base = buf.data() + (size_t)j * 344;
    Sprite s;
    s.w = rd_u16(base + 0);
    s.h = rd_u16(base + 2);
    const uint32_t size = rd_u32(base + 12);
    const bool tiled = base[20] != 0;
    s.px.assign((size_t)s.w * s.h, 0);

    if (tiled) {
      uint32_t tempvalue = 0;
      int run = 0;
      for (int y2 = 0; y2 < (s.h + 63) >> 6; ++y2) {
        for (int x2 = 0; x2 < (s.w + 63) >> 6; ++x2) {
          const int ymax = std::min(y2 * 64 + 64, s.h);
          const int xmax = std::min(x2 * 64 + 64, s.w);
          for (int y = y2 * 64; y < ymax; ++y) {
            for (int x = x2 * 64; x < xmax; x += 4) {
              if (run) {
                wr_u32(&s.px[(size_t)y * s.w + x], tempvalue);
                run -= 4;
              } else {
                tempvalue = rd_u32(buf.data() + cursor);
                cursor += 4;
                if (tempvalue >= 0xff000000u) {
                  run = (int)(tempvalue & 0xffffu);
                  tempvalue = rle_expand(tempvalue);
                  run -= 4;
                }
                wr_u32(&s.px[(size_t)y * s.w + x], tempvalue);
              }
            }
          }
        }
      }
    } else {
      const size_t end = cursor + size;
      size_t k = 0;
      for (size_t p = cursor; p < end; p += 4) {
        uint32_t tempvalue = rd_u32(buf.data() + p);
        if (tempvalue >= 0xff000000u) {
          int run = (int)(tempvalue & 0xffffu);
          tempvalue = rle_expand(tempvalue);
          while (run > 0) {
            wr_u32(&s.px[k * 4], tempvalue);
            run -= 4;
            ++k;
          }
          --k;
        } else {
          wr_u32(&s.px[k * 4], tempvalue);
        }
        ++k;
      }
      cursor = end;
    }
    out.push_back(std::move(s));
  }
  return out;
}

void read_palette_page(const std::vector<uint8_t> &raw, int page,
                       SDL_Color out[256]) {
  size_t off = (size_t)page * (768 + 4) + 4;
  for (int i = 0; i < 256; ++i) {
    out[i].r = (uint8_t)(raw[off++] << 2);
    out[i].g = (uint8_t)(raw[off++] << 2);
    out[i].b = (uint8_t)(raw[off++] << 2);
    out[i].a = 255;
  }
}

} // namespace

std::vector<Background> load_backgrounds(const std::string &assets_dir) {
  // sprite == -1 => the bank is a parallax backdrop: 240 sprites of 64x64
  // assembled as a 20x12 grid. crop_h trims the junk rows below the art
  // (measured per image; 0 = keep all 768).
  struct Pick {
    int bank;
    int sprite;
    int page;
    int crop_h;
  };
  static const Pick kPicks[] = {
      {208, 1, 1, 0}, // bright Mars on starfield (menu backdrop)
      {7, 1, 2, 0},   // green console/schematic screen
      {0, -1, 5, 720},// neon industrial dome
      {1, -1, 6, 702},// teal night cityscape
      {2, -1, 7, 720},// orange Mars terrain
      {3, -1, 8, 720},// sea of clouds + bridge
      {4, -1, 9, 704},// dark purple skyline
  };
  constexpr int kGridW = 20, kGridH = 12, kTile = 64;

  const std::vector<uint8_t> counts = read_file(assets_dir + "/BIN_SPR.DAT");
  const std::vector<uint8_t> pal_raw = read_file(assets_dir + "/PALETTE.BIN");
  if (counts.empty() || pal_raw.empty()) {
    fprintf(stderr, "[launcher] backgrounds: no sprite assets at %s\n",
            assets_dir.c_str());
    return {};
  }

  std::vector<Background> out;
  for (const Pick &p : kPicks) {
    const std::vector<Sprite> sprites = decode_bank(assets_dir, counts, p.bank);
    if (sprites.empty()) {
      fprintf(stderr, "[launcher] backgrounds: bank %d failed to decode\n", p.bank);
      continue;
    }
    Background bg;
    if (p.sprite >= 0) {
      if ((size_t)p.sprite >= sprites.size())
        continue;
      const Sprite &s = sprites[p.sprite];
      bg.w = s.w;
      bg.h = s.h;
      bg.indices = s.px;
    } else {
      bg.w = kGridW * kTile;
      bg.h = kGridH * kTile;
      bg.indices.assign((size_t)bg.w * bg.h, 0);
      const int tiles = std::min((int)sprites.size(), kGridW * kGridH);
      for (int j = 0; j < tiles; ++j) {
        const Sprite &s = sprites[j];
        const int gx = (j % kGridW) * kTile;
        const int gy = (j / kGridW) * kTile;
        const int th = std::min(s.h, kTile), tw = std::min(s.w, kTile);
        for (int y = 0; y < th; ++y)
          memcpy(&bg.indices[(size_t)(gy + y) * bg.w + gx],
                 &s.px[(size_t)y * s.w], tw);
      }
    }
    if (p.crop_h > 0 && p.crop_h < bg.h) {
      bg.h = p.crop_h;
      bg.indices.resize((size_t)bg.w * bg.h);
    }
    if (bg.w <= 0 || bg.h <= 0 || bg.indices.empty())
      continue;
    read_palette_page(pal_raw, p.page, bg.palette);
    out.push_back(std::move(bg));
  }
  return out;
}

bool bake_glyph_faces(silencer::cppx_ui::PipelineHost &host,
                      const std::string &assets_dir) {
  using GF = silencer::cppx_ui::GlyphFonts;
  // FontRegistry::FaceId -> origin bank, with the game's native metrics
  // (game_ui_pipeline.cpp kFaceBakes trackings; Body uses origin's BodySm
  // advance 7 — the tracking the launcher's TTF layout was designed around).
  //
  // pad_top: origin cells are cap-top-aligned with the descender zone at the
  // cell bottom, so flex-centering the line box leaves cap-height text riding
  // 1.5-2px high. Blank rows baked above each glyph re-center the cap ink in
  // the line box; descenders hang below it (baseline behavior — the drawn cell
  // is taller than the box and is not clipped to it).
  struct FaceBake {
    int face;
    int bank;
    float advance;
    float line_height;
    int pad_top;
  };
  static const FaceBake kFaces[] = {
      {0, 133, 7.f, 11.f, 2},  // Body: caps rows 0..7 of 11
      {1, 134, 9.f, 15.f, 2},  // Large: caps 0..10 of 13
      {2, 136, 16.f, 23.f, 2}, // Title: caps 0..19 of 24
      {3, 132, 4.f, 7.f, 1},   // Tiny: full 5px cell
      {4, 135, 11.f, 19.f, 2}, // Heading: caps 0..14 of 17
  };

  const std::vector<uint8_t> counts = read_file(assets_dir + "/BIN_SPR.DAT");
  const std::vector<uint8_t> pal_raw = read_file(assets_dir + "/PALETTE.BIN");
  if (counts.empty() || pal_raw.empty()) {
    fprintf(stderr, "[launcher] glyph fonts: no sprite assets at %s\n",
            assets_dir.c_str());
    return false;
  }
  SDL_Color palette[256];
  read_palette_page(pal_raw, 0, palette);

  bool ok = true;
  for (const FaceBake &fb : kFaces) {
    const std::vector<Sprite> sprites = decode_bank(assets_dir, counts, fb.bank);
    if (sprites.empty()) {
      fprintf(stderr, "[launcher] glyph fonts: bank %d failed to decode\n",
              fb.bank);
      ok = false;
      continue;
    }
    const int ioffset = fb.bank == 132 ? 34 : 33; // glyph index = char - ioffset
    GF::GlyphSrc src[GF::kGlyphCount] = {};
    std::vector<std::vector<uint8_t>> padded;
    padded.reserve(GF::kGlyphCount);
    for (int i = 0; i < GF::kGlyphCount; ++i) {
      const int gi = GF::kFirstChar + i - ioffset;
      if (gi < 0 || (size_t)gi >= sprites.size())
        continue; // space and out-of-bank cells stay blank (advance only)
      const Sprite &sp = sprites[gi];
      if (sp.w < 1 || sp.h < 1 || sp.px.empty())
        continue;
      padded.emplace_back((size_t)sp.w * (sp.h + fb.pad_top), 0);
      std::vector<uint8_t> &px = padded.back();
      memcpy(px.data() + (size_t)sp.w * fb.pad_top, sp.px.data(),
             (size_t)sp.w * sp.h);
      src[i].indices = px.data();
      src[i].w = sp.w;
      src[i].h = sp.h + fb.pad_top;
    }
    ok = host.build_glyph_face(fb.face, src, GF::kGlyphCount, palette,
                               fb.advance, fb.line_height) &&
         ok;
  }
  return ok;
}

} // namespace launcher
