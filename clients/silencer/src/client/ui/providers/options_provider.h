#pragma once

struct SDL_Gamepad;
struct SDL_Window;
class KeyMap;
class RenderDevice;
class ScreenContext;

#include <SDL3/SDL_stdinc.h>

namespace silencer {
namespace client_ui {

struct OptionsProviderValue {
	KeyMap * keymap = nullptr;
	SDL_Window * window = nullptr;
	RenderDevice * renderdevice = nullptr;
	SDL_Gamepad * gamepad = nullptr;
	const Uint32 * tick_count = nullptr;
};

OptionsProviderValue MakeOptionsProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
