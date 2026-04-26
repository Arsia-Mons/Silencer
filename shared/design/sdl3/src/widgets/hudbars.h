// HUD bars — health, shield, fuel, file (sprite bank 95) + frame (bank 94).
#pragma once

#include <cstdint>

namespace silencer {

class SpriteBanks;
class Palette;

struct HudState {
    float health = 1.0f;     // 0..1
    float shield = 1.0f;
    float fuel = 1.0f;
    float files = 0.5f;
    int health_value = 100;
    int shield_value = 100;
    int credits = 1234;
    int ammo = 30;
};

void DrawHudBars(std::uint8_t* dst, int dst_w, int dst_h, const HudState& hud,
                 std::uint32_t state_i, const SpriteBanks& banks, const Palette& pal);

}  // namespace silencer
