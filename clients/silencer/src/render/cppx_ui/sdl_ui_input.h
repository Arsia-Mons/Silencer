#pragma once

// The ONE SDL -> UiInputFrame key vocabulary, shared by every SDL event loop
// that feeds the retained UI (the game's events.cpp, the launcher's main.cpp).
// A second hand-rolled table is how the launcher shipped without modifiers —
// don't fork this; extend it.

#include <SDL3/SDL.h>

#include "ui/input.h"

namespace silencer::cppx_ui {

// SDL keycode -> the runtime's text-editing UiKey vocabulary. Printable
// characters arrive as SDL_EVENT_TEXT_INPUT, not as key events; the letter
// keys here exist only for their Cmd/Ctrl chords (select-all, clipboard).
inline ::ui::UiKey ui_key_from_sdl(SDL_Keycode key) {
  switch (key) {
  case SDLK_BACKSPACE:
    return ::ui::UiKey::Backspace;
  case SDLK_DELETE:
    return ::ui::UiKey::DeleteForward;
  case SDLK_LEFT:
    return ::ui::UiKey::Left;
  case SDLK_RIGHT:
    return ::ui::UiKey::Right;
  case SDLK_HOME:
    return ::ui::UiKey::Home;
  case SDLK_END:
    return ::ui::UiKey::End;
  case SDLK_UP:
    return ::ui::UiKey::Up;
  case SDLK_DOWN:
    return ::ui::UiKey::Down;
  case SDLK_PAGEUP:
    return ::ui::UiKey::PageUp;
  case SDLK_PAGEDOWN:
    return ::ui::UiKey::PageDown;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    return ::ui::UiKey::Enter;
  case SDLK_TAB:
    return ::ui::UiKey::Tab;
  case SDLK_A:
    return ::ui::UiKey::A;
  case SDLK_C:
    return ::ui::UiKey::C;
  case SDLK_V:
    return ::ui::UiKey::V;
  case SDLK_X:
    return ::ui::UiKey::X;
  default:
    return ::ui::UiKey::Unknown;
  }
}

inline uint16_t ui_mods_from_sdl(SDL_Keymod mod) {
  uint16_t out = ::ui::UI_KEY_MOD_NONE;
  if (mod & SDL_KMOD_SHIFT)
    out |= ::ui::UI_KEY_MOD_SHIFT;
  if (mod & SDL_KMOD_CTRL)
    out |= ::ui::UI_KEY_MOD_CTRL;
  if (mod & SDL_KMOD_ALT)
    out |= ::ui::UI_KEY_MOD_ALT;
  if (mod & SDL_KMOD_GUI)
    out |= ::ui::UI_KEY_MOD_SUPER;
  return out;
}

} // namespace silencer::cppx_ui
