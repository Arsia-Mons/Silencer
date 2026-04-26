#include "palette.h"
#include <stdio.h>
#include <string.h>

Color sd_palettes[SD_NUM_PALETTES][SD_PALETTE_SIZE];

bool sd_palette_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        TraceLog(LOG_WARNING, "PALETTE: failed to open %s", path);
        return false;
    }

    /* The design doc says 4-byte header + 11*768; actual files in the
     * shipped assets are 11*768 = 8448 bytes with no header. Detect by
     * size and skip the header only when present. */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size >= 4 + 11 * 768) {
        uint8_t hdr[4];
        if (fread(hdr, 1, 4, f) != 4) { fclose(f); return false; }
    }

    for (int p = 0; p < SD_NUM_PALETTES; p++) {
        uint8_t buf[768];
        if (fread(buf, 1, 768, f) != 768) {
            TraceLog(LOG_WARNING, "PALETTE: short read on sub-palette %d", p);
            fclose(f);
            return false;
        }
        for (int i = 0; i < 256; i++) {
            uint8_t r = buf[i * 3 + 0] << 2;
            uint8_t g = buf[i * 3 + 1] << 2;
            uint8_t b = buf[i * 3 + 2] << 2;
            sd_palettes[p][i] = (Color){ r, g, b, 255 };
        }
        /* Index 0 is transparent color-key. */
        sd_palettes[p][0].a = 0;
    }

    fclose(f);
    TraceLog(LOG_INFO, "PALETTE: loaded %s", path);
    return true;
}

Color sd_index_to_color(int palette_index, int color_index) {
    if (palette_index < 0 || palette_index >= SD_NUM_PALETTES) palette_index = 0;
    if (color_index < 0 || color_index >= SD_PALETTE_SIZE) color_index = 0;
    return sd_palettes[palette_index][color_index];
}

Color sd_effect_brightness(Color in, uint8_t brightness) {
    if (brightness == 128) return in;
    float r = in.r, g = in.g, b = in.b;
    if (brightness > 128) {
        float p = (brightness - 127) / 128.0f;
        r = r * (1 - p) + 255 * p;
        g = g * (1 - p) + 255 * p;
        b = b * (1 - p) + 255 * p;
    } else {
        float p = brightness / 128.0f;
        r *= p; g *= p; b *= p;
    }
    Color out = in;
    out.r = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
    out.g = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
    out.b = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
    return out;
}

Color sd_effect_color(Color in, Color tint) {
    float la = 0.30f * in.r + 0.59f * in.g + 0.11f * in.b;
    float lb = 0.30f * tint.r + 0.59f * tint.g + 0.11f * tint.b;
    float diff = la - lb;
    float r = tint.r + diff;
    float g = tint.g + diff;
    float b = tint.b + diff;
    Color out = in;
    out.r = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
    out.g = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
    out.b = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
    return out;
}
