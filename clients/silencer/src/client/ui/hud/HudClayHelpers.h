#pragma once

#include "clay/clay.h"

#include <SDL3/SDL_stdinc.h>

#include <string>

class Resources;

namespace silencer {
namespace client_ui {

// Player state machine token equivalents the HUD checks against. Mirroring the
// numeric values means HUD code does not need to include player.h.
constexpr Uint8 kPlayerStateHacking = 15;  // matches Player::HACKING
constexpr Uint8 kPlayerStateDying   = 20;  // matches Player::DYING
constexpr Uint8 kPlayerStateDead    = 21;  // matches Player::DEAD

Clay_String ClayStringFromStd(const std::string& text);
Clay_String ClayStringFromCString(const char* text);

// A floating Clay element anchored to the root at logical (x,y) with the given
// size. Used by every HUD sub-builder that places fixed-position widgets.
Clay_ElementDeclaration HudFloatingElement(const char* id, int x, int y, int w, int h);

int SpriteWidth(const Resources& res, Uint8 bank, Uint16 index);
int SpriteHeight(const Resources& res, Uint8 bank, Uint16 index);
int SpriteOffsetX(const Resources& res, Uint8 bank, Uint16 index);
int SpriteOffsetY(const Resources& res, Uint8 bank, Uint16 index);
int SpriteX(const Resources& res, Uint8 bank, Uint16 index, int logicalX = 0);
int SpriteY(const Resources& res, Uint8 bank, Uint16 index, int logicalY = 0);

}  // namespace client_ui
}  // namespace silencer
