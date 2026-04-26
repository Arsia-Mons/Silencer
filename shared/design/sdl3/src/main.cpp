// Silencer design hydration — SDL3
// Renders the main menu using ONLY the design spec at docs/design/
// and the binary assets at shared/assets/.
//
// Dump mode: SILENCER_DUMP_DIR=<dir>  ./silencer_design [assets-path]

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <cassert>

namespace fs = std::filesystem;

// ---------- small util ----------

static std::vector<uint8_t> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "ERROR: cannot open %s\n", p.string().c_str());
        std::exit(1);
    }
    f.seekg(0, std::ios::end);
    auto sz = (std::streamsize)f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> out(sz < 0 ? 0 : (size_t)sz);
    if (sz > 0) f.read((char*)out.data(), sz);
    return out;
}

static uint16_t rd_u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static int16_t rd_i16le(const uint8_t* p) {
    return (int16_t)rd_u16le(p);
}
static uint32_t rd_u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// ---------- palette ----------

struct Palette {
    // 11 sub-palettes, each 256 RGB triples (8-bit channels post-shift).
    std::array<std::array<std::array<uint8_t, 3>, 256>, 11> sub{};

    void load(const fs::path& palette_bin) {
        auto buf = read_file(palette_bin);
        // Per docs/design/palette.md:
        //   color_offset(s) = 4 + s * 772
        //   read 256*(R,G,B) bytes; bounds-check writes past EOF stay 0
        //   raw values are 6-bit (0..63), expand by << 2.
        for (int s = 0; s < 11; ++s) {
            size_t co = 4 + (size_t)s * 772;
            for (int i = 0; i < 256; ++i) {
                size_t off_r = co + (size_t)i * 3 + 0;
                size_t off_g = co + (size_t)i * 3 + 1;
                size_t off_b = co + (size_t)i * 3 + 2;
                uint8_t r = (off_r < buf.size()) ? buf[off_r] : 0;
                uint8_t g = (off_g < buf.size()) ? buf[off_g] : 0;
                uint8_t b = (off_b < buf.size()) ? buf[off_b] : 0;
                sub[s][i][0] = (uint8_t)(r << 2);
                sub[s][i][1] = (uint8_t)(g << 2);
                sub[s][i][2] = (uint8_t)(b << 2);
            }
        }
    }
};

// ---------- sprite ----------

struct Sprite {
    uint16_t w = 0, h = 0;
    int16_t  offset_x = 0, offset_y = 0;
    uint8_t  mode = 0;
    std::vector<uint8_t> pixels; // w*h palette indices (0 = transparent)
};

struct Bank {
    int sprite_count = 0;
    std::vector<Sprite> sprites;
};

// Decode RLE stream into a linear buffer of exactly w*h bytes.
// Returns number of source bytes consumed.
// dword stream: read u32 LE; if (d & 0xFF000000) == 0xFF000000 -> run of
// (d & 0xFFFF) bytes of value ((d>>16)&0xFF). Else literal of 4 bytes.
static size_t rle_decode_linear(const uint8_t* src, size_t src_avail,
                                std::vector<uint8_t>& dst, size_t target) {
    dst.clear();
    dst.reserve(target);
    size_t consumed = 0;
    while (dst.size() < target) {
        if (consumed + 4 > src_avail) {
            // Source exhausted; pad with zeros (defensive; shouldn't happen
            // for the menu sprites).
            dst.resize(target, 0);
            break;
        }
        uint32_t d = rd_u32le(src + consumed);
        consumed += 4;
        if ((d & 0xFF000000u) == 0xFF000000u) {
            uint32_t run_bytes = d & 0x0000FFFFu;
            uint8_t  pixel = (uint8_t)((d >> 16) & 0xFFu);
            for (uint32_t k = 0; k < run_bytes && dst.size() < target; ++k) {
                dst.push_back(pixel);
            }
        } else {
            // 4 literal pixels
            for (int k = 0; k < 4 && dst.size() < target; ++k) {
                dst.push_back((uint8_t)((d >> (8 * k)) & 0xFFu));
            }
        }
    }
    return consumed;
}

// Re-arrange a linear-decoded byte buffer (already w*h bytes long)
// into the final w*h sprite according to `mode`. mode == 0: linear
// (already correct). mode != 0: tile mode (64x64 tiles, partial edges).
//
// Per sprite-banks.md: "Tile: bytes flow in 64×64 tile order — outer
// iteration over tile rows then tile columns; inner iteration over
// rows within a tile, then 4-pixel-wide chunks across the tile."
static void apply_tile_layout(const std::vector<uint8_t>& linear,
                              std::vector<uint8_t>& out,
                              int w, int h, uint8_t mode) {
    out.assign((size_t)w * (size_t)h, 0);
    if (mode == 0) {
        std::memcpy(out.data(), linear.data(), out.size());
        return;
    }
    const int TILE = 64;
    int tile_rows = (h + TILE - 1) / TILE;
    int tile_cols = (w + TILE - 1) / TILE;
    size_t srcp = 0;
    for (int tr = 0; tr < tile_rows; ++tr) {
        int row0 = tr * TILE;
        int rowsThis = std::min(TILE, h - row0);
        for (int tc = 0; tc < tile_cols; ++tc) {
            int col0 = tc * TILE;
            int colsThis = std::min(TILE, w - col0);
            // inner: rows within a tile, then 4-pixel-wide chunks across.
            for (int ry = 0; ry < rowsThis; ++ry) {
                // colsThis is always a multiple of... not guaranteed.
                // The RLE always emits multiples of 4 on runs, and literals
                // are exactly 4. But edge tiles may have odd widths.
                // We iterate in 4-px chunks; partial trailing chunk is
                // truncated. (Bank-6 idx 0 is 640x480 → all tiles full.)
                int cx = 0;
                while (cx < colsThis) {
                    int chunk = std::min(4, colsThis - cx);
                    for (int k = 0; k < chunk; ++k) {
                        if (srcp >= linear.size()) return;
                        out[(size_t)(row0 + ry) * (size_t)w +
                            (size_t)(col0 + cx + k)] = linear[srcp++];
                    }
                    // If chunk < 4, the source still consumed only `chunk`
                    // bytes here because we pre-decoded byte-by-byte.
                    cx += chunk;
                }
            }
        }
    }
}

// Load one bank file SPR_NNN.BIN with a known sprite_count.
static bool load_bank(const fs::path& path, int sprite_count, Bank& bank) {
    auto buf = read_file(path);
    bank.sprite_count = sprite_count;
    bank.sprites.assign(sprite_count, {});

    // Header section: 344 * count + 4 bytes filler.
    constexpr size_t HDR = 344;
    if (buf.size() < (size_t)(HDR * sprite_count + 4)) {
        std::fprintf(stderr, "WARN: bank %s truncated header section\n",
                     path.string().c_str());
        return false;
    }

    // Pixel data starts at 344*count + 4.
    size_t cursor = HDR * (size_t)sprite_count + 4;

    for (int i = 0; i < sprite_count; ++i) {
        const uint8_t* h = buf.data() + (size_t)i * HDR;
        Sprite& sp = bank.sprites[i];
        sp.w = rd_u16le(h + 0);
        sp.h = rd_u16le(h + 2);
        sp.offset_x = rd_i16le(h + 4);
        sp.offset_y = rd_i16le(h + 6);
        // comp_size at offset 12 (mode-0 only); mode at offset 20.
        // We use output-size termination strategy.
        sp.mode = h[20];

        size_t target = (size_t)sp.w * (size_t)sp.h;
        if (target == 0) continue;

        std::vector<uint8_t> linear;
        size_t avail = (cursor < buf.size()) ? (buf.size() - cursor) : 0;
        size_t consumed = rle_decode_linear(buf.data() + cursor, avail,
                                            linear, target);
        cursor += consumed;

        apply_tile_layout(linear, sp.pixels, sp.w, sp.h, sp.mode);
    }
    return true;
}

// ---------- Resources (banks indexed by BIN_SPR.DAT) ----------

struct Resources {
    Palette palette;
    std::vector<int> sprite_count; // [256]
    std::vector<Bank> banks;       // [256], lazily loaded

    void load_index(const fs::path& asset_root) {
        auto idx = read_file(asset_root / "BIN_SPR.DAT");
        sprite_count.assign(256, 0);
        banks.assign(256, {});
        for (int i = 0; i < 256; ++i) {
            sprite_count[i] = idx[(size_t)i * 64 + 2];
        }
    }

    void load_bank_idx(int bank_idx, const fs::path& asset_root) {
        if (bank_idx < 0 || bank_idx >= 256) return;
        if (sprite_count[bank_idx] == 0) return;
        if (!banks[bank_idx].sprites.empty()) return;
        char fname[32];
        std::snprintf(fname, sizeof(fname), "SPR_%03d.BIN", bank_idx);
        load_bank(asset_root / "bin_spr" / fname,
                  sprite_count[bank_idx], banks[bank_idx]);
    }

    const Sprite* get(int bank_idx, int sprite_idx) const {
        if (bank_idx < 0 || bank_idx >= 256) return nullptr;
        const Bank& b = banks[bank_idx];
        if (sprite_idx < 0 || sprite_idx >= (int)b.sprites.size()) return nullptr;
        return &b.sprites[sprite_idx];
    }
};

// ---------- Framebuffer (640x480 indexed) ----------

struct Framebuffer {
    static constexpr int W = 640;
    static constexpr int H = 480;
    std::array<uint8_t, (size_t)W * H> px{};
    void clear() { px.fill(0); }
    void set(int x, int y, uint8_t v) {
        if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
            px[(size_t)y * W + (size_t)x] = v;
        }
    }
};

// ---------- effect brightness LUT ----------
// Build a 256-byte LUT from active sub-palette per palette.md.

static std::array<uint8_t, 256>
build_brightness_lut(const Palette& pal, int active_sub, int brightness) {
    std::array<uint8_t, 256> lut{};
    for (int i = 0; i < 256; ++i) lut[i] = (uint8_t)i;
    if (brightness == 128) return lut; // identity
    for (int i = 2; i < 256; ++i) {
        const auto& src = pal.sub[active_sub][i];
        double r = src[0], g = src[1], b = src[2];
        if (brightness > 128) {
            double t = (brightness - 127) / 128.0;
            if (t > 1.0) t = 1.0;
            r = r + (255.0 - r) * t;
            g = g + (255.0 - g) * t;
            b = b + (255.0 - b) * t;
        } else {
            double t = brightness / 128.0;
            r *= t; g *= t; b *= t;
        }
        // nearest in palette[active][2..255] by squared-Euclidean
        int best_j = i;
        double best_d = 1e30;
        for (int j = 2; j < 256; ++j) {
            const auto& cand = pal.sub[active_sub][j];
            double dr = (double)cand[0] - r;
            double dg = (double)cand[1] - g;
            double db = (double)cand[2] - b;
            double d = dr * dr + dg * dg + db * db;
            if (d < best_d) { best_d = d; best_j = j; }
        }
        lut[i] = (uint8_t)best_j;
    }
    return lut;
}

// ---------- blit ----------

static void blit_sprite(Framebuffer& fb, const Sprite& sp,
                        int top_left_x, int top_left_y,
                        const std::array<uint8_t, 256>* tint_lut) {
    for (int sy = 0; sy < sp.h; ++sy) {
        int y = top_left_y + sy;
        if (y < 0 || y >= Framebuffer::H) continue;
        for (int sx = 0; sx < sp.w; ++sx) {
            int x = top_left_x + sx;
            if (x < 0 || x >= Framebuffer::W) continue;
            uint8_t p = sp.pixels[(size_t)sy * sp.w + (size_t)sx];
            if (p == 0) continue;
            uint8_t out = tint_lut ? (*tint_lut)[p] : p;
            fb.px[(size_t)y * Framebuffer::W + (size_t)x] = out;
        }
    }
}

// Blit an Object-style sprite at (object.x, object.y), applying
// anchor: top_left = (object - sprite_offset). Camera offset is 0
// for the main menu (camera at (320,240) on 640x480 screen).
static void blit_object(Framebuffer& fb, const Sprite& sp,
                        int obj_x, int obj_y,
                        const std::array<uint8_t, 256>* tint_lut) {
    int tx = obj_x - sp.offset_x;
    int ty = obj_y - sp.offset_y;
    blit_sprite(fb, sp, tx, ty, tint_lut);
}

// ---------- DrawText ----------

static void draw_text(Framebuffer& fb, const Resources& res,
                      int x, int y, const std::string& text,
                      int bank, int advance,
                      const std::array<uint8_t, 256>* tint_lut) {
    int ioffset = (bank == 132) ? 34 : 33;
    int xc = 0;
    for (char c : text) {
        unsigned uc = (unsigned char)c;
        if (uc == 0x20 || uc == 0xA0) {
            xc += advance;
            continue;
        }
        int gi = (int)uc - ioffset;
        const Sprite* gs = res.get(bank, gi);
        if (!gs) { xc += advance; continue; }
        // call standard blit pipeline (anchor offset). Glyphs in font
        // banks have offset_x == offset_y == 0 in practice, but use
        // the same path.
        blit_object(fb, *gs, x + xc, y, tint_lut);
        xc += advance;
    }
}

// ---------- main menu composition ----------

struct Overlay {
    int x = 0, y = 0;
    int res_bank = 0xFF;
    int res_index = 0;
    int state_i = 0;
    std::string text;
    int textbank = 135;
    int textwidth = 8;
    int effectbrightness = 128;

    void tick() {
        // Only the bank-208 logo path matters for the menu.
        if (text.empty() && res_bank == 208) {
            if (state_i < 60) {
                res_index = state_i / 2 + 29;
            } else if (state_i < 120) {
                res_index = 60;
            } else {
                res_index = (120 - state_i / 2) + 29;
                if (res_index <= 29) {
                    state_i = -1; // reset; ++ at end -> 0
                }
            }
            // safety clamp
            if (res_index < 29) res_index = 29;
            if (res_index > 60) res_index = 60;
        }
        state_i++;
    }
};

struct Button {
    int x = 0, y = 0;
    std::string text;
    int res_index = 7;
    int effectbrightness = 128;
    // For dump mode we always render in INACTIVE state — no hover.
};

static void render_overlay(Framebuffer& fb, const Resources& res,
                           const Overlay& o, int active_sub) {
    if (!o.text.empty()) {
        // text mode: draw at raw (x, y), no camera offset.
        std::array<uint8_t, 256> identity{};
        for (int i = 0; i < 256; ++i) identity[i] = (uint8_t)i;
        // brightness 128 → identity LUT.
        draw_text(fb, res, o.x, o.y, o.text, o.textbank, o.textwidth,
                  /*tint_lut*/ nullptr);
        return;
    }
    const Sprite* sp = res.get(o.res_bank, o.res_index);
    if (!sp) return;
    if (o.effectbrightness != 128) {
        auto lut = build_brightness_lut(res.palette, active_sub,
                                        o.effectbrightness);
        blit_object(fb, *sp, o.x, o.y, &lut);
    } else {
        blit_object(fb, *sp, o.x, o.y, nullptr);
    }
}

static void render_button(Framebuffer& fb, const Resources& res,
                          const Button& b, int active_sub) {
    const Sprite* sp = res.get(6, b.res_index);
    if (!sp) return;
    std::array<uint8_t, 256> lut{};
    const std::array<uint8_t, 256>* lutp = nullptr;
    if (b.effectbrightness != 128) {
        lut = build_brightness_lut(res.palette, active_sub, b.effectbrightness);
        lutp = &lut;
    }
    blit_object(fb, *sp, b.x, b.y, lutp);

    // text label
    int dst_x = b.x - sp->offset_x;
    int dst_y = b.y - sp->offset_y;
    int xoff = (196 - (int)b.text.size() * 11) / 2;
    int yoff = 8;
    draw_text(fb, res, dst_x + xoff, dst_y + yoff, b.text,
              /*bank*/ 135, /*advance*/ 11, lutp);
}

// ---------- PPM writer ----------

static bool write_ppm_p6(const fs::path& path, const Framebuffer& fb,
                         const Palette& pal, int active_sub) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << Framebuffer::W << " " << Framebuffer::H << "\n255\n";
    std::vector<uint8_t> rgb((size_t)Framebuffer::W * Framebuffer::H * 3);
    for (size_t i = 0; i < (size_t)Framebuffer::W * Framebuffer::H; ++i) {
        uint8_t idx = fb.px[i];
        // Note: idx 0 is treated as transparent at the blit layer, but
        // anything left as 0 in the framebuffer never had a sprite
        // covering it. Map 0 → palette[active][0] (which is (0,0,0) in
        // sub 1 anyway). The active sub-palette resolves the rest.
        rgb[i * 3 + 0] = pal.sub[active_sub][idx][0];
        rgb[i * 3 + 1] = pal.sub[active_sub][idx][1];
        rgb[i * 3 + 2] = pal.sub[active_sub][idx][2];
    }
    f.write((const char*)rgb.data(), (std::streamsize)rgb.size());
    return (bool)f;
}

// ---------- entry ----------

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    fs::path asset_root = "shared/assets";
    if (argc >= 2) asset_root = argv[1];

    Resources res;
    res.palette.load(asset_root / "PALETTE.BIN");
    res.load_index(asset_root);

    // Self-check: print first three RGB triples of sub 0, 1, 2.
    auto print_sub = [&](int s) {
        std::printf("sub %d: ", s);
        for (int i = 0; i < 3; ++i) {
            std::printf("(%d,%d,%d) ",
                        res.palette.sub[s][i][0],
                        res.palette.sub[s][i][1],
                        res.palette.sub[s][i][2]);
        }
        std::printf("\n");
    };
    std::printf("=== Palette self-check (RGB after <<2 expansion) ===\n");
    print_sub(0);
    print_sub(1);
    print_sub(2);

    // Expected per docs/design/palette.md (raw 6-bit values × 4):
    //   sub 0 idx 0: (0,0,0)        idx 1: (0,0,0)        idx 2: (0,0,0)
    //   sub 1 idx 0: (0,0,0)        idx 1: (8,156,0)      idx 2: (64,8,16)
    //   sub 2 idx 0: (0,0,0)        idx 1: (0,0,0)        idx 2: (8,8,8)

    // Load banks needed for the main menu.
    res.load_bank_idx(6,   asset_root); // background plate + button frames
    res.load_bank_idx(133, asset_root); // version-text font
    res.load_bank_idx(135, asset_root); // button-label font
    res.load_bank_idx(208, asset_root); // animated logo

    // Build menu overlays + buttons per screen-main-menu.md.
    Overlay bg;
    bg.x = 0; bg.y = 0;
    bg.res_bank = 6; bg.res_index = 0;

    Overlay logo;
    logo.x = 0; logo.y = 0;
    logo.res_bank = 208; logo.res_index = 29; // initial; will tick.

    Overlay version;
    version.x = 10;
    version.y = 480 - 10 - 7; // = 463
    version.text = "Silencer v00028";
    version.textbank = 133;
    version.textwidth = 11;

    Button btn_tut   { 40,  -134, "Tutorial",         7, 128 };
    Button btn_conn  { 80,  -67,  "Connect To Lobby", 7, 128 };
    Button btn_opts  { 40,   0,   "Options",          7, 128 };
    Button btn_exit  { 0,    67,  "Exit",             7, 128 };

    // Tick simulation to the pinned scene state: bank-208 logo at
    // res_index = 60 (steady-state hold). Per tick.md, at least 120
    // ticks. We tick 120 — first 60 are fade-in, next 60 hold.
    // After 120 ticks of state_i++, state_i = 120; the next tick()
    // would fall into the fade-out branch. We *don't* call that
    // last tick; we render at the state where the most recent tick
    // landed res_index at 60.
    for (int t = 0; t < 120; ++t) {
        logo.tick();
        // Buttons in INACTIVE state — no per-tick state change since
        // none are focused/hovered.
    }
    // After 120 ticks, logo.state_i == 120 and logo.res_index == 60.
    // (At the start of tick 120, state_i was 119 → res_index =
    // 119/2+29 = 88 — wait, that's > 60. Let me reread.) ...
    //
    // Re-reading widget-overlay.md: "state_i < 60: res_index =
    // state_i/2 + 29 (fade in 29→60)". With state_i in [0..59],
    // state_i/2 in [0..29], +29 → res_index in [29..58]. Hmm —
    // doesn't reach 60 in fade-in. Then "60 ≤ state_i < 120:
    // res_index = 60 (hold)". OK so at state_i==60..119 we hold at
    // 60. Then state_i >= 120 → fade out.
    //
    // Our loop runs 120 iterations. Iteration i: tick() reads
    // state_i, computes res_index, then state_i++. So:
    //   it=0:  state_i=0 → 0/2+29=29; state_i becomes 1
    //   it=59: state_i=59 → 59/2+29=58; state_i becomes 60
    //   it=60: state_i=60 → hold branch, res_index=60; state_i=61
    //   it=119: state_i=119 → hold, res_index=60; state_i=120
    // Render now: res_index = 60. ✓

    // Render frame.
    Framebuffer fb;
    fb.clear();

    constexpr int active_sub = 1;

    render_overlay(fb, res, bg,      active_sub);
    render_overlay(fb, res, logo,    active_sub);
    render_overlay(fb, res, version, active_sub);
    render_button(fb,  res, btn_tut,  active_sub);
    render_button(fb,  res, btn_conn, active_sub);
    render_button(fb,  res, btn_opts, active_sub);
    render_button(fb,  res, btn_exit, active_sub);

    // Dump if requested.
    const char* dump_dir = std::getenv("SILENCER_DUMP_DIR");
    if (dump_dir && *dump_dir) {
        fs::create_directories(dump_dir);
        fs::path out = fs::path(dump_dir) / "screen_00.ppm";
        if (!write_ppm_p6(out, fb, res.palette, active_sub)) {
            std::fprintf(stderr, "ERROR: writing PPM %s failed\n",
                         out.string().c_str());
            SDL_Quit();
            return 1;
        }
        std::printf("Wrote %s\n", out.string().c_str());
    }

    SDL_Quit();
    return 0;
}
