#ifndef SD_FONT_H
#define SD_FONT_H

#include <stdint.h>
#include <raylib.h>

/* Draw text using a sprite font bank (132–136), with per-character advance
 * `width`, optional palette-tint `color_index` (0 = no tint), and brightness
 * (128 = neutral, default for design system). */
void sd_draw_text(const char *text, int x, int y, int bank, int width,
                  int color_index, uint8_t brightness);

/* DrawTinyText — auto-centers around `x` using bank 132, width 4. */
void sd_draw_tiny_text(const char *text, int x, int y, int color_index, uint8_t brightness);

/* Draw text with a 1-px drop shadow underneath (matches the announcement style). */
void sd_draw_text_shadowed(const char *text, int x, int y, int bank, int width,
                           int color_index, uint8_t brightness);

/* Glyph height for a font bank (used by Overlay text-mode hit tests). */
int sd_font_glyph_height(int bank);

#endif
