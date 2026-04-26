#ifndef SD_SPRITE_H
#define SD_SPRITE_H

#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>

#define SD_NUM_BANKS 256
#define SD_MAX_SPRITES_PER_BANK 256

typedef struct {
    uint16_t width, height;
    int16_t  offset_x, offset_y;
    /* Indexed (palette index) buffer, width*height bytes, NULL if not loaded. */
    uint8_t *indexed;
    /* Pre-converted RGBA texture (palette 0). 0 if not uploaded. */
    Texture2D texture;
    bool      texture_ready;
} sd_sprite_t;

typedef struct {
    int          count;
    sd_sprite_t *sprites; /* count entries */
} sd_bank_t;

extern sd_bank_t sd_banks[SD_NUM_BANKS];

bool sd_sprites_load(const char *assets_dir);
void sd_sprites_unload(void);

/* Build texture for a single sprite using palette 0. */
void sd_sprite_make_texture(int bank, int index);

/* Draw a sprite at (x, y) (which corresponds to logical anchor; sprite offsets are
 * subtracted, matching the engine convention). Tint applies an EffectColor + brightness. */
void sd_sprite_draw(int bank, int index, int x, int y, Color tint, uint8_t brightness);

/* Draw without tint, just brightness. */
void sd_sprite_draw_b(int bank, int index, int x, int y, uint8_t brightness);

/* Get sprite metrics — returns false if missing. */
bool sd_sprite_metrics(int bank, int index, int *w, int *h, int *ox, int *oy);

#endif
