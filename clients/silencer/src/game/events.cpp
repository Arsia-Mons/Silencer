#include "game.h"
#include "audio.h"
#include "config.h"
#include "player.h"
#include "screen.h"
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

static bool ChatAllowsChar(char ascii){
	if(ascii < 0x20 || ascii > 0x7F) return false;
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
			return false;
	}
	return true;
}

static bool DispatchChatKey(World & world, Player & player, char ascii){
	if(!player.chatActive) return false;
	if(ascii == '\t'){
		player.chatwithteam = !player.chatwithteam;
		return true;
	}
	if(ascii == '\b'){
		size_t len = std::strlen(player.chatText);
		if(len > 0) player.chatText[len - 1] = '\0';
		return true;
	}
	if(ascii == '\n'){
		if(std::strlen(player.chatText) > 0){
			world.SendChat(player.chatwithteam, player.chatText);
		}
		player.chatText[0] = '\0';
		player.chatActive = false;
		return true;
	}
	if(ascii == 0x1B){
		player.chatText[0] = '\0';
		player.chatActive = false;
		return true;
	}
	if(ChatAllowsChar(ascii)){
		size_t len = std::strlen(player.chatText);
		if(len < sizeof(player.chatText) - 1){
			player.chatText[len] = ascii;
			player.chatText[len + 1] = '\0';
		}
		return true;
	}
	return false;
}

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
			if(localplayer->chatActive || interfaceenterfix){
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
				int w = 0, h = 0;
				SDL_GetWindowSize(window, &w, &h);
				ResizeRenderSurface(w, h);
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
				Screen * top = GetTopScreen();
				if(top && !skip && top->HandleTextInput(screenContext, ascii)){
					break;
				}
				Player * localplayer = world.GetPeerPlayer(world.localpeerid);
				if(localplayer && !skip && DispatchChatKey(world, *localplayer, ascii)){
					break;
				}
			}break;
			case SDL_EVENT_KEY_DOWN:{
				OnScancodeDown(event.key.scancode);
				keystate[event.key.scancode] = true;
				{
					Screen * top = GetTopScreen();
					if(top && top->HandleScancodeDown(screenContext, event.key.scancode)){
						break;
					}
				}
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
				Screen * top = GetTopScreen();
				if(top && !skip && top->HandleKeyPress(screenContext, ascii)){
					break;
				}
				Player * localplayer = world.GetPeerPlayer(world.localpeerid);
				if(localplayer && !skip && DispatchChatKey(world, *localplayer, (char)ascii)){
					break;
				}
				if(!skip && HandleInGameMenuKey((char)ascii)){
					break;
				}
			}break;
			case SDL_EVENT_KEY_UP:{
				OnScancodeUp(event.key.scancode);
				keystate[event.key.scancode] = false;
			}break;
			case SDL_EVENT_MOUSE_WHEEL:{
			}break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:{
				if(event.button.button == SDL_BUTTON_LEFT){
					Screen * top = GetTopScreen();
					int w, h;
					SDL_GetWindowSize(window, &w, &h);
					const Surface& uiSurface = GetScreenBuffer();
					Uint16 sx = static_cast<Uint16>((float(event.button.x) / w) * uiSurface.w);
					Uint16 sy = static_cast<Uint16>((float(event.button.y) / h) * uiSurface.h);
					if(top && top->HandleMousePress(screenContext, true, sx, sy)){
						break;
					}
				}
			}break;
			case SDL_EVENT_MOUSE_BUTTON_UP:{
				if(event.button.button == SDL_BUTTON_LEFT){
					Screen * top = GetTopScreen();
					int w, h;
					SDL_GetWindowSize(window, &w, &h);
					const Surface& uiSurface = GetScreenBuffer();
					Uint16 sx = static_cast<Uint16>((float(event.button.x) / w) * uiSurface.w);
					Uint16 sy = static_cast<Uint16>((float(event.button.y) / h) * uiSurface.h);
					if(top && top->HandleMousePress(screenContext, false, sx, sy)){
						break;
					}
				}
			}break;
			case SDL_EVENT_MOUSE_MOTION:{
				Screen * top = GetTopScreen();
				int w, h;
				SDL_GetWindowSize(window, &w, &h);
				const Surface& uiSurface = GetScreenBuffer();
				Uint16 sx = static_cast<Uint16>((float(event.motion.x) / w) * uiSurface.w);
				Uint16 sy = static_cast<Uint16>((float(event.motion.y) / h) * uiSurface.h);
				if(top && top->HandleMouseMove(screenContext, sx, sy)){
					break;
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
		Peer * lp = world.peerlist[world.localpeerid];
		bool isobserver = lp && lp->observer;
		Player * localplayer = world.GetPeerPlayer(world.localpeerid);
		bool playerok = localplayer && !localplayer->chatActive && !localplayer->isbuying && !localplayer->techstationactive;
		if(isobserver || playerok){
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
