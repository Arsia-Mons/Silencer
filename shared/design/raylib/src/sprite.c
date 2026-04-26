#include "sprite.h"
#include "palette.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sd_bank_t sd_banks[SD_NUM_BANKS];

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    if ((long)fread(buf, 1, sz, f) != sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    if (out_size) *out_size = (size_t)sz;
    return buf;
}

static uint16_t rd_u16le(const uint8_t *p) { return p[0] | (p[1] << 8); }
static int16_t  rd_s16le(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd_u32le(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

/* Decode RLE pixel stream — emits up to capacity bytes; returns bytes emitted. */
static size_t decode_rle(const uint8_t *src, size_t csize, uint8_t *dst, size_t capacity) {
    size_t out = 0;
    size_t i = 0;
    while (i + 4 <= csize && out < capacity) {
        uint32_t D = rd_u32le(src + i);
        i += 4;
        if ((D & 0xFF000000u) == 0xFF000000u) {
            size_t runBytes = D & 0x0000FFFFu;
            uint8_t pix = (D >> 16) & 0xFF;
            for (size_t k = 0; k < runBytes && out < capacity; k++) {
                dst[out++] = pix;
            }
        } else {
            for (int k = 0; k < 4 && out < capacity; k++) {
                dst[out++] = (D >> (k * 8)) & 0xFF;
            }
        }
    }
    return out;
}

/* Tile-ordered (64x64 tiles) — re-arrange a linearly-decoded buffer into the
 * sprite's row-major destination. */
static void detile(const uint8_t *src_linear, uint8_t *dst, int w, int h) {
    int tiles_x = (w + 63) / 64;
    int tiles_y = (h + 63) / 64;
    size_t si = 0;
    for (int ty = 0; ty < tiles_y; ty++) {
        int tile_h = (ty == tiles_y - 1) ? (h - ty * 64) : 64;
        for (int tx = 0; tx < tiles_x; tx++) {
            int tile_w = (tx == tiles_x - 1) ? (w - tx * 64) : 64;
            for (int yy = 0; yy < tile_h; yy++) {
                for (int xx = 0; xx < tile_w; xx++) {
                    int dx = tx * 64 + xx;
                    int dy = ty * 64 + yy;
                    dst[dy * w + dx] = src_linear[si++];
                }
            }
        }
    }
}

static bool load_bank(const char *assets_dir, int bank, int sprite_count) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/bin_spr/SPR_%03d.BIN", assets_dir, bank);
    size_t sz;
    uint8_t *raw = read_file(path, &sz);
    if (!raw) {
        TraceLog(LOG_DEBUG, "BANK %d: missing %s", bank, path);
        return false;
    }

    size_t header_total = (size_t)sprite_count * 344;
    if (sz < header_total) { free(raw); return false; }

    sd_banks[bank].count = sprite_count;
    sd_banks[bank].sprites = (sd_sprite_t *)calloc(sprite_count, sizeof(sd_sprite_t));

    /* Pixel data starts at header_total, sprites concatenated in order. */
    size_t data_cursor = header_total;
    for (int i = 0; i < sprite_count; i++) {
        const uint8_t *h = raw + i * 344;
        sd_sprite_t *s = &sd_banks[bank].sprites[i];
        s->width    = rd_u16le(h + 0);
        s->height   = rd_u16le(h + 2);
        s->offset_x = rd_s16le(h + 4);
        s->offset_y = rd_s16le(h + 6);
        uint32_t csize = rd_u32le(h + 12);
        uint8_t  cmode = h[20];

        if (s->width == 0 || s->height == 0) continue;
        if (data_cursor + csize > sz) {
            TraceLog(LOG_WARNING, "BANK %d sprite %d: truncated (need %u, have %zu)",
                     bank, i, csize, sz - data_cursor);
            break;
        }

        size_t pix_count = (size_t)s->width * (size_t)s->height;
        s->indexed = (uint8_t *)calloc(1, pix_count);

        if (cmode == 0) {
            decode_rle(raw + data_cursor, csize, s->indexed, pix_count);
        } else {
            uint8_t *tmp = (uint8_t *)calloc(1, pix_count);
            decode_rle(raw + data_cursor, csize, tmp, pix_count);
            detile(tmp, s->indexed, s->width, s->height);
            free(tmp);
        }
        data_cursor += csize;
    }

    free(raw);
    return true;
}

bool sd_sprites_load(const char *assets_dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/BIN_SPR.DAT", assets_dir);
    size_t sz;
    uint8_t *index = read_file(path, &sz);
    if (!index) {
        TraceLog(LOG_ERROR, "SPRITES: cannot open %s", path);
        return false;
    }
    if (sz < 16384) {
        TraceLog(LOG_ERROR, "SPRITES: BIN_SPR.DAT too small (%zu)", sz);
        free(index);
        return false;
    }

    int loaded = 0;
    for (int b = 0; b < 256; b++) {
        uint8_t n = index[b * 64 + 2];
        if (n == 0) continue;
        if (load_bank(assets_dir, b, n)) loaded++;
    }
    free(index);
    TraceLog(LOG_INFO, "SPRITES: loaded %d banks", loaded);
    return true;
}

void sd_sprites_unload(void) {
    for (int b = 0; b < SD_NUM_BANKS; b++) {
        for (int i = 0; i < sd_banks[b].count; i++) {
            sd_sprite_t *s = &sd_banks[b].sprites[i];
            if (s->indexed) { free(s->indexed); s->indexed = NULL; }
            if (s->texture_ready) { UnloadTexture(s->texture); s->texture_ready = false; }
        }
        if (sd_banks[b].sprites) { free(sd_banks[b].sprites); sd_banks[b].sprites = NULL; }
        sd_banks[b].count = 0;
    }
}

void sd_sprite_make_texture(int bank, int index) {
    if (bank < 0 || bank >= SD_NUM_BANKS) return;
    if (index < 0 || index >= sd_banks[bank].count) return;
    sd_sprite_t *s = &sd_banks[bank].sprites[index];
    if (s->texture_ready || !s->indexed) return;
    int w = s->width, h = s->height;
    if (w <= 0 || h <= 0) return;

    Color *pixels = (Color *)malloc(sizeof(Color) * w * h);
    for (int i = 0; i < w * h; i++) {
        uint8_t idx = s->indexed[i];
        pixels[i] = sd_palettes[0][idx];
        if (idx == 0) pixels[i].a = 0;
    }
    Image img = {
        .data = pixels,
        .width = w,
        .height = h,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    s->texture = LoadTextureFromImage(img);
    SetTextureFilter(s->texture, TEXTURE_FILTER_POINT);
    free(pixels);
    s->texture_ready = true;
}

bool sd_sprite_metrics(int bank, int index, int *w, int *h, int *ox, int *oy) {
    if (bank < 0 || bank >= SD_NUM_BANKS) return false;
    if (index < 0 || index >= sd_banks[bank].count) return false;
    sd_sprite_t *s = &sd_banks[bank].sprites[index];
    if (s->width == 0) return false;
    if (w)  *w  = s->width;
    if (h)  *h  = s->height;
    if (ox) *ox = s->offset_x;
    if (oy) *oy = s->offset_y;
    return true;
}

void sd_sprite_draw_b(int bank, int index, int x, int y, uint8_t brightness) {
    sd_sprite_draw(bank, index, x, y, (Color){0,0,0,0}, brightness);
}

void sd_sprite_draw(int bank, int index, int x, int y, Color tint, uint8_t brightness) {
    if (bank < 0 || bank >= SD_NUM_BANKS) return;
    if (index < 0 || index >= sd_banks[bank].count) return;
    sd_sprite_t *s = &sd_banks[bank].sprites[index];
    if (!s->indexed) return;
    if (!s->texture_ready) sd_sprite_make_texture(bank, index);
    if (!s->texture_ready) return;

    int dx = x - s->offset_x;
    int dy = y - s->offset_y;

    /* If we have a non-trivial tint or non-neutral brightness, do CPU-side
     * recolor to match the engine's per-pixel transforms. */
    bool has_tint = (tint.a != 0);
    if (!has_tint && brightness == 128) {
        DrawTexture(s->texture, dx, dy, WHITE);
        return;
    }

    /* Fall back to CPU regenerate (slow, but only used for occasional widgets). */
    int w = s->width, h = s->height;
    Color *pixels = (Color *)malloc(sizeof(Color) * w * h);
    for (int i = 0; i < w * h; i++) {
        uint8_t idx = s->indexed[i];
        if (idx == 0) { pixels[i] = (Color){0,0,0,0}; continue; }
        Color c = sd_palettes[0][idx];
        if (has_tint) c = sd_effect_color(c, tint);
        if (brightness != 128) c = sd_effect_brightness(c, brightness);
        c.a = 255;
        pixels[i] = c;
    }
    Image img = {
        .data = pixels, .width = w, .height = h, .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    DrawTexture(tex, dx, dy, WHITE);
    UnloadTexture(tex);
    free(pixels);
}
