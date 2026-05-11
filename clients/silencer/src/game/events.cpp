#include "game.h"
#include "runtime.h"
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
			if(localplayer->chatinterfaceid || interfaceenterfix){
				Input zeroinput;
				input = zeroinput;
				interfaceenterfix = true;
			}
			if(localplayer->buyinterfaceid || localplayer->techinterfaceid || interfaceenterfix){
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
				}else if(state == GameState::LOBBYCONNECT){
					// v2 LobbyConnect — no Interface to route through; the
					// active field's buffer lives directly on Game.
					if(!skip) LobbyConnectAppendChar(ascii);
				}else if(state == GameState::LOBBY && LobbyV2ChatActive()){
					// v2 Lobby chat input — buffer lives on Game; the chat
					// sub-interface is the active object whenever no
					// modal/right-side panel takes focus.
					if(!skip) LobbyV2ChatAppendChar(ascii);
				}else if(state == GameState::LOBBY && LobbyV2CreateInputActive()){
					// v2 Lobby game-create panel — routes to the active
					// text input (name or password) per
					// lobby_create_active_field.
					if(!skip) LobbyV2CreateAppendChar(ascii);
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
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					iface->lastsym = event.key.scancode;
					if(!skip){
						iface->ProcessKeyPress(world, ascii);
					}
				}
				// v2 OPTIONSCONTROLS has no Interface on the stack but still
				// needs the next scancode for the rebind capture state
				// machine. TickOptionsControlsV2 consumes + clears it.
				if(state == GameState::OPTIONSCONTROLS && controls_rebind_active_slot >= 0){
					controls_rebind_pending_scancode = (int)event.key.scancode;
				}
				// v2 LOBBYCONNECT also has currentinterface=0 — handle
				// editing scancodes (BACKSPACE, RETURN, ESCAPE, TAB) directly.
				if(state == GameState::LOBBYCONNECT){
					switch(event.key.scancode){
						case SDL_SCANCODE_BACKSPACE: LobbyConnectBackspace(); break;
						case SDL_SCANCODE_RETURN:    LobbyConnectSubmit();    break;
						case SDL_SCANCODE_ESCAPE:    LobbyConnectCancel();    break;
						case SDL_SCANCODE_TAB:       LobbyConnectCycleField(); break;
						default: break;
					}
				}
				// v2 LOBBY chat input — BACKSPACE / RETURN edit the chat
				// buffer when chat is the active sub-interface.
				if(state == GameState::LOBBY && LobbyV2ChatActive()){
					switch(event.key.scancode){
						case SDL_SCANCODE_BACKSPACE: LobbyV2ChatBackspace(); break;
						case SDL_SCANCODE_RETURN:    LobbyV2ChatSubmit();    break;
						default: break;
					}
				}
				// v2 LOBBY game-create panel — same shape, RETURN fires the
				// Create button (legacy gamecreateinterface->buttonenter).
				if(state == GameState::LOBBY && LobbyV2CreateInputActive()){
					switch(event.key.scancode){
						case SDL_SCANCODE_BACKSPACE: LobbyV2CreateBackspace(); break;
						case SDL_SCANCODE_RETURN:    LobbyV2CreateSubmit();    break;
						default: break;
					}
				}
				// v2 LOBBY tab-nav arrow keys — cycle nav_cursor between
				// chat / character / right-panel regions. Only fires when
				// no text input has focus (otherwise arrow keys would steal
				// from in-input cursor motion).
				if(state == GameState::LOBBY && !LobbyV2ChatActive() &&
				   !LobbyV2CreateInputActive()){
					switch(event.key.scancode){
						case SDL_SCANCODE_LEFT:
						case SDL_SCANCODE_UP:    LobbyV2NavPrev(); break;
						case SDL_SCANCODE_RIGHT:
						case SDL_SCANCODE_DOWN:  LobbyV2NavNext(); break;
						default: break;
					}
				}
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
					int w, h;
					SDL_GetWindowSize(window, &w, &h);
					int lx = (int)((float(event.button.x) / w) * 640);
					int ly = (int)((float(event.button.y) / h) * 480);
					if(IsV2ModalActive()){
						// v2 modal intercepts clicks before any per-state path.
						DispatchV2ModalClick(lx, ly);
						break;
					}
					if(active_runtime && active_runtime->DispatchMouseDown(lx, ly)){
						// Runtime consumed the click.
					}else if(state == GameState::OPTIONSAUDIO){
						DispatchOptionsAudioV2Click(lx, ly);
					}else if(state == GameState::OPTIONSCONTROLS){
						DispatchOptionsControlsV2Click(lx, ly);
					}else if(state == GameState::UPDATING){
						DispatchUpdateV2Click(lx, ly);
					}else if(state == GameState::MISSIONSUMMARY){
						DispatchMissionSummaryV2Click(lx, ly);
					}else if(state == GameState::LOBBYCONNECT){
						DispatchLobbyConnectV2Click(lx, ly);
					}else if(state == GameState::LOBBY){
						DispatchLobbyV2Click(lx, ly);
					}else{
						Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
						if(iface){
							iface->ProcessMousePress(world, true, (float)lx, (float)ly);
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
				int w, h;
				SDL_GetWindowSize(window, &w, &h);
				int lx = (int)((float(event.motion.x) / w) * 640);
				int ly = (int)((float(event.motion.y) / h) * 480);
				if(state == GameState::MAINMENU || state == GameState::OPTIONS || state == GameState::OPTIONSDISPLAY || state == GameState::OPTIONSAUDIO || state == GameState::OPTIONSCONTROLS || state == GameState::UPDATING || state == GameState::MISSIONSUMMARY || state == GameState::LOBBYCONNECT || state == GameState::LOBBY || IsV2ModalActive()){
					// Feed v2 render hover styling. Always update; the v2
					// render pass reads ui_v2_mouse_{x,y} next frame. Also
					// applies whenever a v2 modal is overlaid on a non-v2 state.
					ui_v2_mouse_x = lx;
					ui_v2_mouse_y = ly;
				}else{
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
		if(localplayer && !localplayer->chatinterfaceid && !localplayer->buyinterfaceid){
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
