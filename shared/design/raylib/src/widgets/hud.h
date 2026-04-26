#ifndef SD_HUD_H
#define SD_HUD_H

#include <stdbool.h>

typedef struct {
    int health, max_health;
    int shield, max_shield;
    int fuel,   max_fuel;
    int files,  max_files;
    int ammo;
    int credits;
    bool fuel_low;
    int  state_i;     /* renderer tick counter */
} sd_hud_state_t;

void sd_hud_draw(const sd_hud_state_t *s);

#endif
