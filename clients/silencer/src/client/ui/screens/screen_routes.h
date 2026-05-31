#pragma once

#include <SDL3/SDL_stdinc.h>

#include <memory>

class Screen;

namespace silencer {
namespace client_ui {

bool IsScreenState(Uint8 state);
bool ScreenStatePlaysMenuMusic(Uint8 state);
std::unique_ptr<Screen> CreateScreenForState(Uint8 state);

}  // namespace client_ui
}  // namespace silencer
