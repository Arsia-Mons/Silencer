#ifndef SD_PALETTE_H
#define SD_PALETTE_H

#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>

#define SD_NUM_PALETTES 11
#define SD_PALETTE_SIZE 256

extern Color sd_palettes[SD_NUM_PALETTES][SD_PALETTE_SIZE];

bool sd_palette_load(const char *path);

/* EffectBrightness — linear lerp toward white (>128) or black (<128). */
Color sd_effect_brightness(Color in, uint8_t brightness);

/* EffectColor — luminance-preserving tint. */
Color sd_effect_color(Color in, Color tint);

/* Convert palette index to RGBA color from a given palette. Index 0 → fully transparent. */
Color sd_index_to_color(int palette_index, int color_index);

#endif
