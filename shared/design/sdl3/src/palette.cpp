#include "palette.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace silencer {

bool Palette::Load(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "palette: failed to open %s\n", path.c_str());
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long fsz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(fsz);
    size_t got = std::fread(buf.data(), 1, fsz, f);
    std::fclose(f);
    if (got != static_cast<size_t>(fsz)) {
        std::fprintf(stderr, "palette: short read\n");
        return false;
    }

    // The spec is explicit that offsets are 4 + s*(768+4) and that the
    // file is shorter than the math expects — we mirror that and zero
    // any out-of-range read.
    for (int s = 0; s < kSubPalettes; ++s) {
        long off = 4 + s * (768 + 4);
        for (int i = 0; i < kColors; ++i) {
            long base = off + i * 3;
            uint8_t r = 0, g = 0, b = 0;
            if (base + 0 < fsz) r = buf[base + 0];
            if (base + 1 < fsz) g = buf[base + 1];
            if (base + 2 < fsz) b = buf[base + 2];
            // 6-bit channels expanded to 8-bit by left-shift 2.
            tables_[s][i].r = static_cast<uint8_t>(r << 2);
            tables_[s][i].g = static_cast<uint8_t>(g << 2);
            tables_[s][i].b = static_cast<uint8_t>(b << 2);
        }
    }
    return true;
}

void Palette::BuildBrightnessLut(int brightness, std::array<uint8_t, 256> &out) const {
    // 128 == neutral. Scale RGB by brightness/128 and find nearest
    // matching palette entry under the active sub-palette. Entry 0
    // is reserved transparent and maps to itself.
    out[0] = 0;
    const auto &pal = tables_[active_];
    for (int i = 1; i < kColors; ++i) {
        float scale = static_cast<float>(brightness) / 128.0f;
        float tr = std::min(255.0f, pal[i].r * scale);
        float tg = std::min(255.0f, pal[i].g * scale);
        float tb = std::min(255.0f, pal[i].b * scale);
        int best = i;
        double best_d = 1e18;
        for (int j = 1; j < kColors; ++j) {
            double dr = pal[j].r - tr;
            double dg = pal[j].g - tg;
            double db = pal[j].b - tb;
            double d = dr * dr + dg * dg + db * db;
            if (d < best_d) {
                best_d = d;
                best = j;
            }
        }
        out[i] = static_cast<uint8_t>(best);
    }
}

}  // namespace silencer
