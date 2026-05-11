#include "game.h"
#include "renderer.h"
#include "runtime.h"
#include "ingame_chat.h"
#include "ingame_buy.h"
#include "ingame_tech.h"
#include "audio.h"
#include "config.h"
#include "interface.h"
#include "player.h"
#include "world.h"
#include <cstring>

// Captureless lambdas decay to function pointers, so this table costs nothing
// at runtime vs. the old hand-written cascade. New actions get one row here
// (and one row in ACTION_TABLE) — that's the entire fan-out for adding one.
typedef bool& (*InputField)(Input&);
static const struct { Action a; InputField field; } INPUT_FIELDS[] = {
	{ Action::MoveUp,        [](Input& i) -> bool& { return i.keymoveup;        } },
	{ Action::MoveDown,      [](Input& i) -> bool& { return i.keymovedown;      } },
	{ Action::MoveLeft,      [](Input& i) -> bool& { return i.keymoveleft;      } },
	{ Action::MoveRight,     [](Input& i) -> bool& { return i.keymoveright;     } },
	{ Action::LookUpLeft,    [](Input& i) -> bool& { return i.keylookupleft;    } },
	{ Action::LookUpRight,   [](Input& i) -> bool& { return i.keylookupright;   } },
	{ Action::LookDownLeft,  [](Input& i) -> bool& { return i.keylookdownleft;  } },
	{ Action::LookDownRight, [](Input& i) -> bool& { return i.keylookdownright; } },
	{ Action::Jump,          [](Input& i) -> bool& { return i.keyjump;          } },
	{ Action::Jetpack,       [](Input& i) -> bool& { return i.keyjetpack;       } },
	{ Action::Activate,      [](Input& i) -> bool& { return i.keyactivate;      } },
	{ Action::Use,            [](Input& i) -> bool& { return i.keyuse;          } },
	{ Action::Fire,          [](Input& i) -> bool& { return i.keyfire;          } },
	{ Action::Chat,          [](Input& i) -> bool& { return i.keychat;          } },
	{ Action::NextInv,       [](Input& i) -> bool& { return i.keynextinv;       } },
	{ Action::NextCam,       [](Input& i) -> bool& { return i.keynextcam;       } },
	{ Action::PrevCam,       [](Input& i) -> bool& { return i.keyprevcam;       } },
	{ Action::Detonate,      [](Input& i) -> bool& { return i.keydetonate;      } },
	{ Action::Disguise,      [](Input& i) -> bool& { return i.keydisguise;      } },
	{ Action::NextWeapon,    [](Input& i) -> bool& { return i.keynextweapon;    } },
	{ Action::Weapon1,       [](Input& i) -> bool& { return i.keyweapon[0];     } },
	{ Action::Weapon2,       [](Input& i) -> bool& { return i.keyweapon[1];     } },
	{ Action::Weapon3,       [](Input& i) -> bool& { return i.keyweapon[2];     } },
	{ Action::Weapon4,       [](Input& i) -> bool& { return i.keyweapon[3];     } },
	{ Action::UiUp,          [](Input& i) -> bool& { return i.keyup;            } },
	{ Action::UiDown,        [](Input& i) -> bool& { return i.keydown;          } },
	{ Action::UiLeft,        [](Input& i) -> bool& { return i.keyleft;          } },
	{ Action::UiRight,       [](Input& i) -> bool& { return i.keyright;         } },
};

void Game::UpdateInputState(Input & input){
	float mousex;
	float mousey;
	Uint32 mousestate = SDL_GetMouseState(&mousex, &mousey);
	PollGamepadState();
	gamepadstate.mouseButtons = 0;
	for(int b = 1; b <= 5; ++b){
		if(mousestate & SDL_BUTTON_MASK(b)) gamepadstate.mouseButtons |= (1u << (b - 1));
	}
	for(const auto& f : INPUT_FIELDS){
		f.field(input) = keymap.IsPressed(f.a, keystate, gamepadstate);
	}
	input.mousex = (Uint16)mousex;
	input.mousey = (Uint16)mousey;
	input.mousedown = SDL_BUTTON_LEFT & mousestate ? true : false;

	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	if(localplayer){
		if(input.keynextweapon && !localplayer->input.keynextweapon){
			switch(localplayer->currentweapon){
				case 0:
					if(localplayer->laserammo > 0){
						input.keyweapon[1] = true;
					}else
					if(localplayer->rocketammo > 0){
						input.keyweapon[2] = true;
					}else
					if(localplayer->flamerammo > 0){
						input.keyweapon[3] = true;
					}
				break;
				case 1:
					if(localplayer->rocketammo > 0){
						input.keyweapon[2] = true;
					}else
					if(localplayer->flamerammo > 0){
						input.keyweapon[3] = true;
					}else{
						input.keyweapon[0] = true;
					}
				break;
				case 2:
					if(localplayer->flamerammo > 0){
						input.keyweapon[3] = true;
					}else{
						input.keyweapon[0] = true;
					}
				break;
				case 3:
					input.keyweapon[0] = true;
				break;
			}
		}
		if(!world.replay.IsPlaying()){
			if(interfaceenterfix && !keystate[SDL_SCANCODE_RETURN]){
				interfaceenterfix = false;
			}
			if(world.ingame_chat_active || interfaceenterfix){
				Input zeroinput;
				input = zeroinput;
				interfaceenterfix = true;
			}
			if(localplayer->isbuying || localplayer->techstationactive || interfaceenterfix){
				Input zeroinput;
				zeroinput.keyactivate = input.keyactivate;
				zeroinput.keymoveleft = input.keymoveleft;
				zeroinput.keymoveright = input.keymoveright;
				input = zeroinput;
				interfaceenterfix = true;
			}
		}
	}
}

void Game::OpenFirstGamepad(){
	if(gamepad){ SDL_CloseGamepad(gamepad); gamepad = nullptr; }
	int count = 0;
	SDL_JoystickID* ids = SDL_GetGamepads(&count);
	if(ids){
		if(count > 0) gamepad = SDL_OpenGamepad(ids[0]);
		SDL_free(ids);
	}
	gamepadstate.connected = (gamepad != nullptr);
	if(gamepadstate.connected){
		// Auto-switch to the gamepad keybind profile, but only if the current
		// profile isn't already gamepad-derived (e.g. "gamepad-custom" saved
		// from a previous session — don't clobber it with the built-in).
		const char* cur = Config::GetInstance().active_keybind_profile;
		std::string curStr = (cur && *cur) ? cur : "default";
		bool alreadyGamepad = (curStr.find("gamepad") != std::string::npos);
		if(!alreadyGamepad){
			prevGamepadProfile = curStr;
			std::strncpy(Config::GetInstance().active_keybind_profile, "gamepad",
			             sizeof(Config::GetInstance().active_keybind_profile) - 1);
			Config::GetInstance().active_keybind_profile[
				sizeof(Config::GetInstance().active_keybind_profile) - 1] = '\0';
			LoadActiveKeymap(keymap);
		}
	}
}

void Game::PollGamepadState(){
	if(!gamepad){
		// Try (cheaply) to pick up a pad plugged in mid-session.
		OpenFirstGamepad();
	}
	gamepadstate.connected = (gamepad != nullptr);
	gamepadstate.buttons   = 0;
	for(auto& a : gamepadstate.axes) a = 0;
	if(!gamepad) return;
	for(int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b){
		if(SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)b)){
			gamepadstate.buttons |= (1u << b);
		}
	}
	for(int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a){
		gamepadstate.axes[a] = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)a);
	}
}

bool Game::HandleSDLEvents(void){
	if(world.dedicatedserver.active){
		return true;
	}
	if(headless || tui){
		// SDL_INIT_VIDEO was skipped, so `window` is NULL; event handlers below
		// would deref it via SDL_GetWindowSize. Bail before SDL_PollEvent.
		return true;
	}
	SDL_Event event;
	while(SDL_PollEvent(&event) > 0){
		switch(event.type){
			case SDL_EVENT_WINDOW_RESIZED:{
				// SDL3 GPU swapchain resizes automatically.
			}break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:{
				if(!world.replay.IsPlaying()){
					Audio::GetInstance().Unmute();
				}
				minimized = false;
			}break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:{
				if(!world.replay.IsPlaying()){
					Audio::GetInstance().Mute(25);
				}
				minimized = true;
			}break;
			case SDL_EVENT_WINDOW_MINIMIZED:{
				minimized = true;
			}break;
			case SDL_EVENT_WINDOW_MAXIMIZED:{
				minimized = false;
			}break;
			case SDL_EVENT_WINDOW_RESTORED:{
				minimized = false;
			}break;
			case SDL_EVENT_TEXT_INPUT:{
				char ascii = event.text.text[0] & 0x7F;
				bool skip = true;
				if(ascii >= 0x20 && ascii <= 0x7F){
					skip = false;
				}
				switch(ascii){
					case '[':
					case '\\':
					case ']':
					case '^':
					case '_':
					case '`':
					case '{':
					case '|':
					case '}':
					case '~':
						skip = true;
					break;
				}
				if(IsV2ModalActive()){
					// v2 modal owns text input — append into password buf
					// (no-op for MESSAGE-kind modals).
					if(!skip) DispatchV2ModalText((char)ascii);
				}else if(IngameChatOverlay().Active()){
					// In-game chat overlay absorbs text input while typing
					// a chat message.
					if(!skip) IngameChatOverlay().DispatchText((char)ascii);
				}else if(active_runtime && !skip && active_runtime->DispatchTextInput((char)ascii)){
					// Runtime consumed the char (LobbyConnect / Lobby chat /
					// Lobby game-create active field).
				}else{
					Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
					if(iface){
						//iface->lastsym = ascii;
						if(!skip){
							iface->ProcessKeyPress(world, ascii);
						}
					}
				}
			}break;
			case SDL_EVENT_KEY_DOWN:{
				OnScancodeDown(event.key.scancode);
				keystate[event.key.scancode] = true;
			bool skip = true;
			Uint8 ascii;
			switch(event.key.scancode){
					case SDL_SCANCODE_LEFT:
						ascii = 1;
						skip = false;
					break;
					case SDL_SCANCODE_RIGHT:
						ascii = 2;
						skip = false;
					break;
					case SDL_SCANCODE_UP:
						ascii = 3;
						skip = false;
					break;
					case SDL_SCANCODE_DOWN:
						ascii = 4;
						skip = false;
					break;
					case SDL_SCANCODE_BACKSPACE:
						ascii = '\b';
						skip = false;
					break;
					case SDL_SCANCODE_TAB:
						ascii = '\t';
						skip = false;
					break;
					case SDL_SCANCODE_RETURN:
						ascii = '\n';
						skip = false;
					break;
					case SDL_SCANCODE_ESCAPE:
						ascii = 0x1B;
						skip = false;
					break;
					default:{
						if(keymap.IsPressed(Action::MoveUp, keystate, gamepadstate)){
							ascii = 3;
							skip = false;
						}
						if(keymap.IsPressed(Action::MoveDown, keystate, gamepadstate)){
							ascii = 4;
							skip = false;
						}
					}break;
				}
				if(IsV2ModalActive()){
					// Active v2 modal absorbs Enter/Esc/Backspace; per-state
					// editing keys never fire.
					DispatchV2ModalKey((int)event.key.scancode);
					break;
				}
				if(IngameChatOverlay().Active()){
					// In-game chat overlay absorbs RETURN/ESC/TAB/BACKSPACE
					// (and swallows everything else so movement keys don't
					// fire while typing).
					IngameChatOverlay().DispatchKey((int)event.key.scancode);
					break;
				}
				if(IngameBuyOverlay().Active()){
					// In-game buy menu absorbs UP/DOWN (nav) + RETURN (buy)
					// and swallows the rest so weapon/movement keys don't
					// fire while the menu is open.
					IngameBuyOverlay().DispatchKey((int)event.key.scancode);
					break;
				}
				if(IngameTechOverlay().Active()){
					// In-game tech menu absorbs UP/DOWN (nav) + RETURN
					// (repair / virus) and swallows the rest.
					IngameTechOverlay().DispatchKey((int)event.key.scancode);
					break;
				}
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					iface->lastsym = event.key.scancode;
					if(!skip){
						iface->ProcessKeyPress(world, ascii);
					}
				}
				// v2 runtimes can handle scancodes (rebind capture,
				// LobbyConnect editing keys, Lobby chat/create editing,
				// Lobby arrow-key nav). Routed via active_runtime.
				if(active_runtime) active_runtime->DispatchKeyDown((int)event.key.scancode);
			}break;
			case SDL_EVENT_KEY_UP:{
				OnScancodeUp(event.key.scancode);
				keystate[event.key.scancode] = false;
			}break;
			case SDL_EVENT_MOUSE_WHEEL:{
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					if(event.wheel.y > 0){
						iface->ProcessMouseWheelUp(world);
					}else
					if(event.wheel.y < 0){
						iface->ProcessMouseWheelDown(world);
					}
				}
			}break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:{
				if(event.button.button == SDL_BUTTON_LEFT){
					int logical_w, logical_h, scale;
					Renderer::ComputeUIDims(window, logical_w, logical_h, scale);
					// v2 logical coords = pixel coords / scale. Matches the
					// Path B renderer (P22) which blits sprites at integer
					// scale into a window-sized screenbuffer.
					int lx_v2 = (int)(event.button.x / scale);
					int ly_v2 = (int)(event.button.y / scale);
					if(IsV2ModalActive()){
						// v2 modal intercepts clicks before any per-state path.
						DispatchV2ModalClick(lx_v2, ly_v2);
						break;
					}
					if(active_runtime && active_runtime->DispatchMouseDown(lx_v2, ly_v2, logical_w, logical_h, scale)){
						// Runtime consumed the click.
					}else{
						// Legacy iface renders into a fixed 640x480 screenbuffer
						// that SDL stretches to the window — map window pixels
						// back to 640x480 logical space.
						int w, h;
						SDL_GetWindowSize(window, &w, &h);
						int lx_legacy = (int)((float(event.button.x) / w) * 640);
						int ly_legacy = (int)((float(event.button.y) / h) * 480);
						Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
						if(iface){
							iface->ProcessMousePress(world, true, (float)lx_legacy, (float)ly_legacy);
						}
					}
				}
			}break;
			case SDL_EVENT_MOUSE_BUTTON_UP:{
				if(event.button.button == SDL_BUTTON_LEFT){
					if(IsV2ModalActive()){
						// v2 modal absorbed the mouse-down; mirror by absorbing
						// the mouse-up too so the underlying iface doesn't see
						// a stale up event.
						break;
					}
					if(state == GameState::MAINMENU || state == GameState::OPTIONS || state == GameState::OPTIONSDISPLAY || state == GameState::OPTIONSAUDIO || state == GameState::OPTIONSCONTROLS || state == GameState::UPDATING || state == GameState::MISSIONSUMMARY || state == GameState::LOBBYCONNECT || state == GameState::LOBBY){
						// v2 fires on mouse-down (matches preview); nothing to
						// do on mouse-up.
					}else{
						Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
						if(iface){
							int w, h;
							SDL_GetWindowSize(window, &w, &h);
							iface->ProcessMousePress(world, false, (float(event.button.x) / w) * 640, (float(event.button.y) / h) * 480);
						}
					}
				}
			}break;
			case SDL_EVENT_MOUSE_MOTION:{
				if(state == GameState::MAINMENU || state == GameState::OPTIONS || state == GameState::OPTIONSDISPLAY || state == GameState::OPTIONSAUDIO || state == GameState::OPTIONSCONTROLS || state == GameState::UPDATING || state == GameState::MISSIONSUMMARY || state == GameState::LOBBYCONNECT || state == GameState::LOBBY || IsV2ModalActive()){
					// Feed v2 render hover styling. Always update; the v2
					// render pass reads ui_v2_mouse_{x,y} next frame. Also
					// applies whenever a v2 modal is overlaid on a non-v2 state.
					int logical_w, logical_h, scale;
					Renderer::ComputeUIDims(window, logical_w, logical_h, scale);
					ui_v2_mouse_x = (int)(event.motion.x / scale);
					ui_v2_mouse_y = (int)(event.motion.y / scale);
				}else{
					int w, h;
					SDL_GetWindowSize(window, &w, &h);
					int lx = (int)((float(event.motion.x) / w) * 640);
					int ly = (int)((float(event.motion.y) / h) * 480);
					Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
					if(iface){
						iface->ProcessMouseMove(world, (float)lx, (float)ly);
					}
				}
			}break;
			case SDL_EVENT_GAMEPAD_ADDED:{
				// SDL3 doesn't auto-open gamepads; without this, a pad plugged
				// in mid-session is only picked up by PollGamepadState's
				// fallback (which itself stops working if `gamepad` is stale).
				if(!gamepad) OpenFirstGamepad();
			}break;
			case SDL_EVENT_GAMEPAD_REMOVED:{
				// SDL keeps the SDL_Gamepad* valid until SDL_CloseGamepad, so
				// we must explicitly close + null on removal — otherwise
				// PollGamepadState's `if(!gamepad)` reopen guard never fires.
				if(gamepad && event.gdevice.which == SDL_GetGamepadID(gamepad)){
					SDL_CloseGamepad(gamepad);
					gamepad = nullptr;
					gamepadstate.connected = false;
					// Restore the pre-gamepad keybind profile if we auto-switched.
					if(!prevGamepadProfile.empty()){
						std::strncpy(Config::GetInstance().active_keybind_profile,
						             prevGamepadProfile.c_str(),
						             sizeof(Config::GetInstance().active_keybind_profile) - 1);
						Config::GetInstance().active_keybind_profile[
							sizeof(Config::GetInstance().active_keybind_profile) - 1] = '\0';
						LoadActiveKeymap(keymap);
						prevGamepadProfile.clear();
					}
				}
			}break;
			case SDL_EVENT_QUIT:
				return false;
			break;
		}
	}
	return true;
}

void Game::OnScancodeDown(int sc){
	if(sc == quitscancode){
		Player * localplayer = world.GetPeerPlayer(world.localpeerid);
		if(localplayer && !world.ingame_chat_active && !localplayer->isbuying){
			if(world.quitstate == 0){
				world.quitstate = 1;
			}else
			if(world.quitstate == 2){
				world.quitstate = 3;
			}
		}
	}
	if(sc == SDL_SCANCODE_F1){
		world.showplayerlist = true;
	}
	if(sc == SDL_SCANCODE_F2){
		world.showteamcolors = !world.showteamcolors;
	}
	if(sc == SDL_SCANCODE_F5){
		ambienceMixer.LoadRandomGameMusic();
		ambienceMixer.PlayMusic(world.resources.gamemusic);
	}
	if(sc == SDL_SCANCODE_F4){
		if(Config::GetInstance().music){
			if(Audio::GetInstance().MusicPaused()){
				Audio::GetInstance().ResumeMusic();
				world.ShowTopMessage("           MUSIC RESUMED");
			}else{
				Audio::GetInstance().PauseMusic();
				world.ShowTopMessage("          *MUSIC PAUSED*");
			}
		}
	}
	if(sc == SDL_SCANCODE_F9){
		world.debugoverlay = !world.debugoverlay;
		world.ShowTopMessage(world.debugoverlay ? "        DEBUG OVERLAY ON" : "       DEBUG OVERLAY OFF");
	}
}

void Game::OnScancodeUp(int sc){
	if(sc == quitscancode){
		if(world.quitstate == 1){
			world.quitstate = 2;
		}
		if(world.quitstate == 3){
			world.quitstate = 0;
		}
	}
	if(sc == SDL_SCANCODE_F1){
		world.showplayerlist = false;
	}
}
