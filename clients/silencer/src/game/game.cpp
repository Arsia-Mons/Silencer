#include "game.h"
#include "controldispatch.h"
#include "sdl3gpubackend.h"
#include "tuibackend.h"
#include <math.h>
#include "overlay.h"
#include "interface.h"
#include "textbox.h"
#include "textinput.h"
#include "button.h"
#include "toggle.h"
#include "state.h"
#include "selectbox.h"
#include "scrollbar.h"
#include "os.h"
#include "team.h"
#include "player.h"
#include "playerai.h"
#include "terminal.h"
#include "config.h"
#include "cocoawrapper.h"
#include "sha1.h"
#include "mapfetch.h"
#include "actordef.h"
#include "behaviortree.h"
#include "gasloader.h"
#include "screen.h"
#include "updaterstage2.h"
#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"
#include "main_menu.h"
#include "options.h"
#include "options_display.h"
#include "options_audio.h"
#include "options_controls.h"
#include "update.h"
#include "mission_summary.h"
#include "lobby_connect.h"
#include "lobby_shell.h"
#include "modal_stack.h"
#include "ingame_chat.h"
#include "ingame_buy.h"
#include "ingame_tech.h"
#include "runtime.h"
#include "lobby_shell.h"
#include "lobbygame.h"
#include "serializer.h"
#include "audio.h"
#include "renderdevice.h"
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_scancode.h>
#include <algorithm>
#include <stdio.h>

using namespace GameState;

#ifndef SILENCER_LOBBY_HOST
#define SILENCER_LOBBY_HOST "127.0.0.1"
#endif
#ifndef SILENCER_LOBBY_PORT
#define SILENCER_LOBBY_PORT 517
#endif
#ifndef SILENCER_VERSION
#define SILENCER_VERSION "00025"
#endif
#ifndef SILENCER_MAP_API_URL
#define SILENCER_MAP_API_URL "http://127.0.0.1:8080"
#endif

Game::Game() : renderer(world), screenbuffer(640, 480),
               mapDownloader(world),
               ambienceMixer(world, renderer, mapDownloader, fade_i),
               screenContext(*this, world, renderer, world.lobby, keymap, updater, ambienceMixer, mapDownloader, window, renderdevice){
	world.SetVersion(SILENCER_VERSION);
	frames = 0;
	fps = 0;
	state = MAINMENU;
	stateisnew = true;
	fade_i = 0;
	sharedstate = 0;
	currentlobbygameid = 0;
	lastannouncedgameid = 0;
	lastannouncedstatus = 0;
	joininggame = false;
	memset(keystate, 0, sizeof(keystate));
	gamepad = nullptr;
	singleplayermessage = 0;
	updatetitle = true;
	currentinterface = 0;
	minimized = false;
	window = 0;
	renderdevice = nullptr;
	memset(palettecolors, 0, sizeof(palettecolors));
	nextstateprocessed = false;
#ifdef OUYA
	quitscancode = SDL_SCANCODE_HOME;
#else
	quitscancode = SDL_SCANCODE_ESCAPE;
#endif
	fullscreentoggled = false;
	replayfile = 0;
	tui_prev_mouse_x = 0;
	tui_prev_mouse_y = 0;
	tui_prev_mouse_down = false;
	tui_have_prev_mouse = false;
	controlPort = 0;
	tuiInputPort = 0;
	headless = false;
	tui = false;
	paused = false;
	stepFramesRemaining = 0;
	stepWallclockDeadlineMs = 0;
	preview_screen[0] = 0;
	preview_impl[0] = 0;
	dump_ppm_path[0] = 0;
	preview_scale = 1;
	ui_v2_modal_stack = std::make_unique<ui::v2::ModalStack>(world, ui_v2_mouse_x, ui_v2_mouse_y);
	ui_v2_ingame_chat = std::make_unique<ui::v2::IngameChat>(world);
	ui_v2_ingame_buy  = std::make_unique<ui::v2::IngameBuy>(world);
	ui_v2_ingame_tech = std::make_unique<ui::v2::IngameTech>(world);
}

Game::~Game(){
	// Join background download threads before tearing down SDL so they don't
	// reference freed resources. The MapDownloader destructor would also do
	// this, but it runs after SDL teardown — call it explicitly here.
	mapDownloader.JoinAndShutdown();
	// Bring the control server down first. Stop() runs the shutdown drain we
	// registered at Load() time, fulfilling promises for both queued and
	// pendingWaits commands so handler threads can unblock from fut.get() before
	// we join them. Doing this before tearing down anything else keeps members
	// pendingWaits/etc alive while the drain runs.
	controlserver.Stop();
	inputserver.Stop();
	if(renderdevice){
		renderdevice->Shutdown();
		delete renderdevice;
		renderdevice = nullptr;
	}
	if(window){
		SDL_DestroyWindow(window);
	}
	world.resources.UnloadSounds();
	Audio::GetInstance().Close();
	MIX_Quit();
	if(gamepad){ SDL_CloseGamepad(gamepad); gamepad = nullptr; }
	SDL_Quit();
}

bool Game::Load(char * cmdline){
	// CLI overrides for the lobby host / port — applied AFTER Config::Load
	// below so the on-disk config doesn't clobber them. Empty strings / 0
	// mean "no override; use the config value".
	char lobbyHostOverride[256] = {0};
	int  lobbyPortOverride = 0;
	if((cmdline = strtok(cmdline, " "))){
		do{
			if(strncmp(cmdline, "-s", 2) == 0){ // dedicated server
				setbuf(stdout, NULL);
				setbuf(stderr, NULL);
				char * lobbyaddress = strtok(NULL, " ");
				char * lobbyport = strtok(NULL, " ");
				char * gameid = strtok(NULL, " ");
				char * accountid = strtok(NULL, " ");
				char * gameport = strtok(NULL, " "); // optional: explicit UDP bind port (for Docker port mapping)
				if(gameid && accountid && lobbyaddress && lobbyport){
					unsigned short bindport = (gameport && atoi(gameport) > 0) ? (unsigned short)atoi(gameport) : 0;
					world.Listen(bindport);
					world.lobby.Connect(lobbyaddress, atoi(lobbyport));
					do{
						world.lobby.DoNetwork();
						if(world.lobby.state == Lobby::RESOLVED){
							world.lobby.Connect(lobbyaddress, atoi(lobbyport));
						}
						if(world.lobby.state == Lobby::DISCONNECTED || world.lobby.state == Lobby::CONNECTIONFAILED || world.lobby.state == Lobby::RESOLVEFAILED){
							return false;
						}
						SDL_Delay(1);
					}while(world.lobby.state != Lobby::CONNECTED);
					world.gameplaystate = World::INLOBBY;
					/*User * user;
					do{
						user = world.lobby.GetUserInfo(2);
						world.lobby.DoNetwork();
					}while(user->retrieving);
					printf("name: %s, techslots: %d\n", user->name, user->agency[0].techslots);*/
					world.dedicatedserver.Start(lobbyaddress, atoi(lobbyport), atoi(gameid), atoi(accountid));
					char filename[256];
					sprintf(filename, "replays/%d.zsr", atoi(gameid));
					world.replay.BeginRecording(filename);
					if(world.replay.IsRecording()){
						world.replay.WriteHeader(world);
					}
					State * newstateobject = static_cast<State *>(world.CreateObject(ObjectTypes::STATE));
					sharedstate = newstateobject->id;
					newstateobject->state = 0;
					state = NONE;
				}
			}else
			if(strncmp(cmdline, "-r", 2) == 0){ // play replay
				replayfile = strtok(NULL, " ");
				GoToState(REPLAYGAME);
			}
			else if(strcmp(cmdline, "--control-port") == 0){
				char * portstr = strtok(NULL, " ");
				if(portstr){
					controlPort = atoi(portstr);
				}
			}
			else if(strcmp(cmdline, "--tui-input-port") == 0){
				char * portstr = strtok(NULL, " ");
				if(portstr){
					tuiInputPort = atoi(portstr);
				}
			}
			else if(strcmp(cmdline, "--headless") == 0){
				headless = true;
			}
			else if(strcmp(cmdline, "--tui") == 0){
				tui = true;
			}
			else if(strcmp(cmdline, "--lobby-host") == 0){
				char * host = strtok(NULL, " ");
				if(host){
					strncpy(lobbyHostOverride, host, sizeof(lobbyHostOverride) - 1);
					lobbyHostOverride[sizeof(lobbyHostOverride) - 1] = '\0';
				}
			}
			else if(strcmp(cmdline, "--lobby-port") == 0){
				char * portstr = strtok(NULL, " ");
				if(portstr){
					lobbyPortOverride = atoi(portstr);
				}
			}
			else if(strcmp(cmdline, "--preview-screen") == 0){
				char * name = strtok(NULL, " ");
				if(name){
					strncpy(preview_screen, name, sizeof(preview_screen) - 1);
					preview_screen[sizeof(preview_screen) - 1] = 0;
				}
			}
			else if(strcmp(cmdline, "--preview-impl") == 0){
				char * impl = strtok(NULL, " ");
				if(impl){
					strncpy(preview_impl, impl, sizeof(preview_impl) - 1);
					preview_impl[sizeof(preview_impl) - 1] = 0;
				}
			}
			else if(strcmp(cmdline, "--dump-ppm") == 0){
				char * path = strtok(NULL, " ");
				if(path){
					strncpy(dump_ppm_path, path, sizeof(dump_ppm_path) - 1);
					dump_ppm_path[sizeof(dump_ppm_path) - 1] = 0;
				}
			}
			else if(strcmp(cmdline, "--preview-scale") == 0){
				char * s = strtok(NULL, " ");
				if(s){
					int n = atoi(s);
					if(n >= 1 && n <= 8) preview_scale = n;
				}
			}
		}while((cmdline = strtok(0, " ")));
	}
	Config::GetInstance().Load();
	if(lobbyHostOverride[0]){
		strncpy(Config::GetInstance().lobbyhost, lobbyHostOverride, sizeof(Config::GetInstance().lobbyhost) - 1);
		Config::GetInstance().lobbyhost[sizeof(Config::GetInstance().lobbyhost) - 1] = '\0';
	}
	if(lobbyPortOverride > 0){
		Config::GetInstance().lobbyport = lobbyPortOverride;
	}
	LoadActiveKeymap(keymap);
	if(world.dedicatedserver.active){
		// Dedicated server: SDL3 always initialises the timer subsystem; no flags needed.
		if(!SDL_Init(0)){
			printf("Could not initialize SDL %s\n", SDL_GetError());
			return false;
		}
	}
	if(!world.dedicatedserver.active){
		// SDL_INIT_GAMEPAD is opt-in; without it SDL_GetGamepads() returns
		// nothing. Headless builds (CI / control-socket smoke tests) skip it
		// since they don't need controller input. TUI mode keeps audio so the
		// engine's normal mixer path works, but skips video and gamepad.
		Uint32 sdlflags;
		if(tui){
			sdlflags = SDL_INIT_AUDIO;
		}else if(headless){
			sdlflags = 0;
		}else{
			sdlflags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD;
		}
		if(!SDL_Init(sdlflags)){
			printf("Could not initialize SDL %s\n", SDL_GetError());
			return false;
		}
		if(!headless && !tui) OpenFirstGamepad();
		printf("Loading palette...\n");
		if(!renderer.palette.SetPalette(0)){
			return false;
		}
		if(tui){
			if(!MIX_Init()){
				printf("Could not initialize SDL_mixer: %s\n", SDL_GetError());
			}
			if(!Audio::GetInstance().Init(this)){
				printf("Could not initialize audio\n");
			}
			Audio::GetInstance().SetMusicVolume(Config::GetInstance().musicvolume);
			// No SDL_AddTimer (FPS counter title bar is window-only).
			// No window — TUIBackend connects to the TS frontend over TCP.
			if(!SetupRenderDevice()){
				printf("Could not initialize TUI render device\n");
				return false;
			}
			SetColors(renderer.palette.GetColors());
		}else if(!headless){
			if(!MIX_Init()){
				printf("Could not initialize SDL_mixer: %s\n", SDL_GetError());
			}
			if(!Audio::GetInstance().Init(this)){
				printf("Could not initialize audio\n");
			}
			Audio::GetInstance().SetMusicVolume(Config::GetInstance().musicvolume);
			SDL_AddTimer(1000, TimerCallback, this);
			//SDL_EnableUNICODE(true);
			//SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
			//screen = SDL_SetVideoMode(640, 480, 8, SDL_DOUBLEBUF | SDL_SWSURFACE);
			window = SDL_CreateWindow("Silencer", screenbuffer.w, screenbuffer.h, SDL_WINDOW_RESIZABLE | (Config::GetInstance().fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
			SDL_StartTextInput(window);
			if(!SetupRenderDevice()){
				printf("Could not initialize GPU render device\n");
				return false;
			}
			SetColors(renderer.palette.GetColors());
			//SDL_Flip(screen);
		}
		// Headless mode skips SetColors() above, so palettecolors[] starts zeroed.
		// It is first populated by SetColors() in the MAINMENU state handler in
		// Loop(). Screenshot ops route to PostFrameReplies() AFTER Tick() runs,
		// so the palette is always populated by the time a screenshot is captured.
	}
	printf("Loading resources...\n");
	if(!world.resources.Load(*this, world.dedicatedserver.active)){
		printf("Could not load resources\n");
		return false;
	}
	// GAS is loaded inside resources.Load() — rebuild buyable items now that
	// GASLoader::Get().items is populated (World constructor ran too early).
	world.LoadBuyableItems();
	printf("Resources loaded\n");
	lasttick = SDL_GetTicks();
	if(controlPort > 0){
		auto drainPendingWaits = [this](){
			for(auto& w : pendingWaits){
				if(!w.cmd.reply) continue;
				ControlReply rpl;
				rpl.id = w.cmd.id;
				rpl.ok = false;
				rpl.code = "INTERNAL";
				rpl.error = "server stopping";
				w.cmd.reply->set_value(rpl);
			}
			pendingWaits.clear();
		};
		if(!controlserver.Start(controlPort, drainPendingWaits)){
			fprintf(stderr, "[control] failed to start; continuing without\n");
		}
	}
	if(tui && tuiInputPort > 0){
		if(!inputserver.Start(tuiInputPort)){
			fprintf(stderr, "[input] failed to start; continuing without\n");
		}
	}
	return true;
}

bool Game::SetupRenderDevice(void){
	if(tui){
		TUIBackend *backend = new TUIBackend();
		if(!backend->Init(nullptr)){
			delete backend;
			return false;
		}
		renderdevice = backend;
		// TUIBackend ignores SetScaleFilter; safe to call.
		renderdevice->SetScaleFilter(false);
		return true;
	}
	SDL3GPUBackend *backend = new SDL3GPUBackend();
	if(!backend->Init(window)){
		delete backend;
		return false;
	}
	renderdevice = backend;
	renderdevice->SetScaleFilter(Config::GetInstance().scalefilter);
	return true;
}

void Game::Present(void){
	if(renderdevice){
		renderdevice->UploadFrame(screenbuffer.pixels.data(), screenbuffer.w, screenbuffer.h);
		renderdevice->Present();
	}
}

void Game::LoadProgressCallback(int progress, int totalprogressitems){
	if(world.dedicatedserver.active){
		return;
	}
	HandleSDLEvents();
	if(SDL_GetTicks() - lasttick >= 100){
		int width = 500;
		int widthp = (float(progress) / totalprogressitems) * width;
		int barx = (640 - width) / 2;
		int bary = (480 - 32) / 2;
		renderer.DrawFilledRectangle(&screenbuffer, barx, bary, barx + width, bary + 32, 101);
		if(widthp > 0){
			for(int c = 0; c < 13; c++){
				int x0 = barx + (c * widthp) / 13;
				int x1 = barx + ((c + 1) * widthp) / 13;
				if(x1 > x0) renderer.DrawFilledRectangle(&screenbuffer, x0, bary, x1, bary + 32, 101 + c);
			}
		}
		Present();
		lasttick = SDL_GetTicks();
	}
}

void Game::SetColors(SDL_Color * colors){
	memcpy(palettecolors, colors, 256 * sizeof(SDL_Color));
	if(renderdevice){
		renderdevice->SetPalette(colors, 256);
	}
}

Uint32 Game::TimerCallback(void * userdata, SDL_TimerID timerID, Uint32 interval){
	Game * game = static_cast<Game *>(userdata);
	game->updatetitle = true;
	game->fps = game->frames;
	return 1000;
}

bool Game::Loop(void){
	if(updater.IsStage2Spawned()){
		// Stage-2 child has been spawned and is waiting on our PID to exit.
		// Tell main to unwind so ~Game() tears down SDL/audio cleanly.
		return false;
	}
	if(quitRequested) return false;
	DrainControlQueue();
	unsigned int wait = GASLoader::Get().gameengine.tickIntervalMs;
	if(updatetitle){
		if(!headless && window){
			char title[128];
			sprintf(title, "Silencer - %d FPS  Latency: %d ms [%d]  B/s: D:%d U:%d", fps, world.GetPingTime(), (int)world.snapshotqueue.size(), world.totalbytesread, world.totalbytessent);
			SDL_SetWindowTitle(window, title);
		}
		updatetitle = false;
		frames = 1;
		world.totalbytesread = 0;
		world.totalbytessent = 0;
	}
	/*while(SDL_GetTicks() - lasttick <= wait){
		//SDL_Delay(1);
		world.DoNetwork();
	}*/
	world.DoNetwork();
	Uint64 tickcheck = SDL_GetTicks();
	if(world.replay.IsPlaying()){
		wait = GASLoader::Get().gameengine.tickIntervalMs * world.replay.speed;
		if(world.replay.ffmpeg && world.replay.ffmpegvideo){
			tickcheck = world.replay.tick;
			if(!tickcheck){
				lasttick = 0;
			}
			world.replay.tick += 20; // 50 fps
		}
	}
	while(lasttick <= tickcheck && tickcheck - lasttick > wait){
		//printf("%d\n", tickcheck - lasttick);
		if(paused){
			bool budgetFrames = stepFramesRemaining > 0;
			bool budgetMs = stepWallclockDeadlineMs > 0 && SDL_GetTicks() < stepWallclockDeadlineMs;
			if(!budgetFrames && !budgetMs){
				lasttick = tickcheck; // freeze the catch-up clock
				break;
			}
			if(stepFramesRemaining > 0) --stepFramesRemaining;
			if(stepWallclockDeadlineMs > 0 && SDL_GetTicks() >= stepWallclockDeadlineMs){
				stepWallclockDeadlineMs = 0;
			}
		}
		world.systemcameraactive[0] = false;
		world.systemcameraactive[1] = false;
		world.DoNetwork();
		// In TUI mode, world.localinput is built from the binary input channel
		// rather than SDL keyboard polling. Two layered sources, each
		// latest-wins:
		//   - scancode bitmask  → Game::keystate → UpdateInputState (same
		//                          path as native SDL, so the user's keymap
		//                          profile is honored)
		//   - action snapshot   → ORed on top so programmatic / CLI clients
		//                          can drive Input fields directly without
		//                          knowing the keymap.
		// Edge events (menu nav, text input) still arrive via the control
		// socket "key" op and bypass this path entirely.
		if(tui){
			Uint8 newkeystate[SDL_SCANCODE_COUNT];
			if(inputserver.LatestScancodes(newkeystate)){
				// Edge-detect: feed press/release transitions through the
				// same handlers the SDL path uses, so the in-game ESC
				// quitstate machine, F1 player-list, debug overlay etc.
				// behave identically with a TUI keyboard.
				for(int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc){
					bool was = keystate[sc] != 0;
					bool now = newkeystate[sc] != 0;
					if(was == now) continue;
					if(now) OnScancodeDown(sc);
					else    OnScancodeUp(sc);
				}
				memcpy(keystate, newkeystate, sizeof(keystate));
			}
			UpdateInputState(world.localinput);
			Input action;
			if(inputserver.LatestAction(action)){
				world.localinput.keymoveup        |= action.keymoveup;
				world.localinput.keymovedown      |= action.keymovedown;
				world.localinput.keymoveleft      |= action.keymoveleft;
				world.localinput.keymoveright     |= action.keymoveright;
				world.localinput.keylookupleft    |= action.keylookupleft;
				world.localinput.keylookupright   |= action.keylookupright;
				world.localinput.keylookdownleft  |= action.keylookdownleft;
				world.localinput.keylookdownright |= action.keylookdownright;
				world.localinput.keynextinv       |= action.keynextinv;
				world.localinput.keynextcam       |= action.keynextcam;
				world.localinput.keyprevcam       |= action.keyprevcam;
				world.localinput.keydetonate      |= action.keydetonate;
				world.localinput.keyjump          |= action.keyjump;
				world.localinput.keyjetpack       |= action.keyjetpack;
				world.localinput.keyactivate      |= action.keyactivate;
				world.localinput.keyuse           |= action.keyuse;
				world.localinput.keyfire          |= action.keyfire;
				world.localinput.keydisguise      |= action.keydisguise;
				world.localinput.keynextweapon    |= action.keynextweapon;
				world.localinput.keyup            |= action.keyup;
				world.localinput.keydown          |= action.keydown;
				world.localinput.keyleft          |= action.keyleft;
				world.localinput.keyright         |= action.keyright;
				world.localinput.keychat          |= action.keychat;
				world.localinput.keyweapon[0]     |= action.keyweapon[0];
				world.localinput.keyweapon[1]     |= action.keyweapon[1];
				world.localinput.keyweapon[2]     |= action.keyweapon[2];
				world.localinput.keyweapon[3]     |= action.keyweapon[3];
				if(action.mousex != 0xFFFF) world.localinput.mousex = action.mousex;
				if(action.mousey != 0xFFFF) world.localinput.mousey = action.mousey;
				world.localinput.mousedown        |= action.mousedown;
			}
			// Latest mouse snapshot (TUI cell→pixel conversion already done
			// host-side). Overwrites whatever UpdateInputState wrote — in TUI
			// mode SDL_GetMouseState returns 0,0 since there's no video.
			Uint16 mx, my;
			bool md;
			if(inputserver.LatestMouse(mx, my, md)){
				world.localinput.mousex    = mx;
				world.localinput.mousey    = my;
				world.localinput.mousedown = md;
				// Edge-detect transitions and dispatch to the current
				// interface — mirrors HandleSDLEvents' SDL_EVENT_MOUSE_*
				// handlers (game.cpp ~6275). Without this the menu UI
				// never sees the mouse, since iface state is driven by
				// ProcessMousePress / ProcessMouseMove rather than
				// world.localinput reads.
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					bool moved = !tui_have_prev_mouse ||
					             mx != tui_prev_mouse_x ||
					             my != tui_prev_mouse_y;
					bool downChanged = !tui_have_prev_mouse ||
					                   md != tui_prev_mouse_down;
					if(moved){
						iface->ProcessMouseMove(world, mx, my);
					}
					if(downChanged){
						iface->ProcessMousePress(world, md, mx, my);
					}
				}
				tui_prev_mouse_x    = mx;
				tui_prev_mouse_y    = my;
				tui_prev_mouse_down = md;
				tui_have_prev_mouse = true;
			}
		} else {
			UpdateInputState(world.localinput);
			// If a rebind slot is waiting for input, zero out gamepad-driven
			// localinput so button presses don't leak into gameplay/UI actions.
			if(currentinterface){
				Interface* rebindIface = (Interface*)world.GetObjectFromId(currentinterface);
				if(rebindIface && rebindIface->disabled){
					world.localinput.keyup = world.localinput.keydown =
					world.localinput.keyleft = world.localinput.keyright = false;
				}
			}
			TickGamepadMenuNav();
		}
		world.SendInput();
		if(!Tick()){
			return false;
		}
		if(!world.replay.IsPlaying() || (world.replay.IsPlaying() && world.gameplaystate == World::INGAME)){
			world.Tick();
			TickRumble();
		}
		if(!world.dedicatedserver.active){
			renderer.Tick();
		}
		if(world.gameplaystate == World::INGAME){
			Uint8 newambiencelevel = renderer.GetAmbienceLevel();
			if(newambiencelevel != ambienceMixer.oldambiencelevel || fade_i <= 15){
				SDL_Color * colors = renderer.palette.GetColors();
				if(fade_i <= 15){
					colors = renderer.palette.GetTempPalette();
				}
				SDL_Color * ambiencepalette = renderer.palette.CopyWithBrightness(colors, newambiencelevel, 2, 114);
				SetColors(ambiencepalette);
				renderer.palette.CalculateLighted(newambiencelevel);
				ambienceMixer.oldambiencelevel = newambiencelevel;
			}
		}
		fade_i++;
		if(fade_i >= 16){
			fade_i = 16;
		}
		lasttick += wait;
	}
	// Tick multi-frame waits AFTER the sim loop so wait_frames --n 1 and
	// step --frames 1 see at least one sim tick before resolving. Putting this
	// inside DrainControlQueue (which runs before the sim loop) made the
	// counter race the enqueue and resolve with zero ticks.
	ControlDispatch::TickWaits(*this);
	world.DoNetwork();
	if(!world.dedicatedserver.active){
		screenbuffer.Clear(0);
		world.DoNetwork();
		if(active_runtime){
			// Runtime owns the frame: declarative tree → Layout → Render.
			// Bypasses renderer.Draw entirely (no world objects to walk).
			Uint64 now = SDL_GetTicks();
			float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
			ui_v2_last_ticks = now;
			active_runtime->Render(screenbuffer, renderer, ui_v2_mouse_x, ui_v2_mouse_y, dt);
		}else{
			renderer.Draw(&screenbuffer, 1 - (float(tickcheck - lasttick) / wait));
		}
		// v2 modal overlay — top of stack draws on top of whatever the
		// per-state path produced. Empty stack is a no-op.
		RenderV2ModalOverlay();
#ifdef POSIX
		if(world.replay.IsPlaying() && world.replay.ffmpeg && world.replay.ffmpegvideo && deploymessageshown){
			Uint8 buffer[640 * 480 * 3];
			int i = 0;
			int j = 0;
			for(int y = screenbuffer.h; y > 0; y--){
				for(int x = screenbuffer.w; x > 0; x--){
					buffer[i++] = palettecolors[screenbuffer.pixels[j]].r;
					buffer[i++] = palettecolors[screenbuffer.pixels[j]].g;
					buffer[i++] = palettecolors[screenbuffer.pixels[j]].b;
					j++;
				}
			}
			fwrite(buffer, sizeof(buffer), 1, world.replay.ffmpeg);
		}
#endif
		/*char fpstext[16];
		sprintf(fpstext, "%d", fps);
		renderer.DrawText(&screenbuffer, 10, 30, fpstext, 133, 7);*/
		if(minimized){
			SDL_Delay(wait);
		}
		world.DoNetwork();
		//Uint32 drawtick = SDL_GetTicks();
		if(!headless || tui) Present();
		// In TUI mode, the frontend owning our render output may disconnect
		// (terminal closed, host process killed). TUIBackend tears the socket
		// down on any write failure; we observe that here and exit cleanly
		// rather than burning CPU rendering frames nobody reads.
		if(tui && renderdevice && !renderdevice->IsAlive()){
			quitRequested = true;
		}
		// SDL3GPUBackend's swapchain Present blocks on vsync (~16 ms) so the
		// non-TUI loop self-throttles. TUIBackend writes to a TCP socket that
		// never blocks the engine, so without an explicit cap the loop runs
		// thousands of times per second. Sleep enough to land at ~30 fps —
		// well above the 24 Hz sim rate (catch-up loop handles the sim
		// cadence) and well below "burn a CPU core for no reason".
		if(tui) SDL_Delay(33);
		PostFrameReplies();
		//Uint32 afterdrawtick = SDL_GetTicks();
		/*if(1 || afterdrawtick - drawtick > wait){
			printf("frame took %d ms to present\n", afterdrawtick - drawtick);
		}*/
	}else{
		SDL_Delay(1);
	}
	frames++;
	return true;
}

bool Game::Tick(void){
	if(screenStackPendingTeardown){
		while(!screenStack.empty()) PopScreen();
		screenStackPendingTeardown = false;
	}
	TickActiveScreen();
	if(!world.dedicatedserver.active){
		if(world.lobby.state == Lobby::AUTHENTICATED){
			// 0 = main lobby, 1 = pregame (game-specific lobby, waiting for
			// match start), 2 = playing (gameplaystate == INGAME).
			Uint32 targetgid = 0;
			Uint8 targetstatus = 0;
			if(currentlobbygameid != 0 && world.state == World::CONNECTED){
				targetgid = currentlobbygameid;
				targetstatus = (world.gameplaystate == World::INGAME) ? 2 : 1;
			}
			if(targetgid != lastannouncedgameid || targetstatus != lastannouncedstatus){
				world.lobby.SendSetGame(targetgid, targetstatus);
				lastannouncedgameid = targetgid;
				lastannouncedstatus = targetstatus;
			}
		}else{
			lastannouncedgameid = 0;
			lastannouncedstatus = 0;
		}
	}
	if(world.dedicatedserver.active && state != HOSTGAME){
		if(world.dedicatedserver.nopeerstime >= GASLoader::Get().gameengine.nopeersTimeoutTicks){
			world.dedicatedserver.SendHeartBeat(world, 2);
			return false;
		}
		if(sharedstate){
			State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
			if(sharedstateobject && sharedstateobject->state == 0 && world.peercount >= 1 && world.AllPeersReady(world.localpeerid) && world.AllPeersLoadedGameInfo() && world.AllPeersDownloadedMap()){
				sharedstateobject->state = 1;
				GoToState(INGAME);
			}
		}
		if(world.gameplaystate == World::INLOBBY){
			mapDownloader.ProcessMapDownload();
			// Ready-button text refresh ("Waiting..." vs "Ready") moved into
			// GameJoinPanel::Tick — runs each frame from LobbyScreen::Tick.
		}
		/*Peer * localpeer = world.peerlist[world.localpeerid];
		if(localpeer){
			if(localpeer->gameinfoloaded && !world.dedicatedserver.checkedhavemap){
				if(mapDownloader.FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).size() > 0){
					localpeer->mapdownloaded = true;
					world.SendPeerList();
				}
				world.dedicatedserver.checkedhavemap = true;
			}
		}*/
	}
	if(!sharedstate && !world.IsAuthority()){
		//printf("no shared state!");
		for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
			Object * object = *it;
			if(object->type == ObjectTypes::STATE){
				sharedstate = object->id;
				break;
			}
		}
	}
	if(sharedstate && !world.IsAuthority()){
		State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
		if(sharedstateobject && sharedstateobject->state != sharedstateobject->oldstate){
			switch(sharedstateobject->state){
				case 1:
					GoToState(INGAME);
				break;
				case 2:
					if(state != INGAME){
						GoToState(INGAME);
						//state = INGAME;
						//stateisnew = true;
					}
				break;
			}
			sharedstateobject->oldstate = sharedstateobject->state;
		}
	}
	
	if(world.gameplaystate == World::INGAME && (state == INGAME || state == SINGLEPLAYERGAME || state == TESTGAME)){
		ambienceMixer.UpdateAmbienceChannels();
		if(!headless) SDL_HideCursor();
	}else{
		if(!headless) SDL_ShowCursor();
	}

	if(!headless && window){
		if(keystate[SDL_SCANCODE_RALT] && keystate[SDL_SCANCODE_RETURN]){
			if(!fullscreentoggled){
				if(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN){
					SDL_SetWindowFullscreen(window, false);
				}else{
					SDL_SetWindowFullscreen(window, true);
				}
				fullscreentoggled = true;
			}
		}else{
			fullscreentoggled = false;
		}
	}
	
	switch(state){
		case FADEOUT: TickFadeOut(); break;
		case MAINMENU:{
			if(stateisnew){
				world.Disconnect();
				world.gameplaystate = World::NONE;
				world.lobby.Disconnect();
				UnloadGame();
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				// v2 MainMenuRuntime owns the surface: render, click
				// dispatch, and per-button hot_t animation. Palette +
				// camera setup mirrors what MainMenuScreen::Build used
				// to do.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(MAINMENU);
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				if(active_runtime) active_runtime->Tick();
			}
		}break;
		case LOBBYCONNECT:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				world.lobby.ClearGames();
				world.lobby.state = Lobby::WAITING;
				// LobbyConnectRuntime owns the surface, input buffers,
				// textbox lines, and lobby state machine. Palette 2 +
				// camera mirror LobbyConnectScreen::Build's
				// ResetPresentation(2).
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(LOBBYCONNECT);
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				if(active_runtime) active_runtime->Tick();
			}
		}break;
		case LOBBY:{
			if(stateisnew){
				world.lobby.ForgetAllUserInfo();
				world.gameplaystate = World::INLOBBY;
				UnloadGame();
				world.Disconnect();
				world.choosingtech = false;
				world.lobby.channelchanged = true;
				// LobbyRuntime owns the surface + per-panel state +
				// CreateGame state machine. Palette 2 + camera (320, 240)
				// mirror LobbyScreen::Build's ResetPresentation(2).
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(LOBBY);
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				if(active_runtime) active_runtime->Tick();
			}
		}break;
		case UPDATING:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				// UpdateRuntime owns the surface + STAGING transition.
				// Palette 2 mirrors the legacy ResetPresentation(2) call.
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(UPDATING);
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				if(active_runtime) active_runtime->Tick();
			}
		}break;
		case INGAME: TickInGame(); break;
		case MISSIONSUMMARY:{
			if(stateisnew){
				UnloadGame();
				world.Disconnect();
				// MissionSummaryRuntime owns the surface. Palette 1 +
				// camera (320, 240) mirror MissionSummaryScreen::Build.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				// Ensure the lobby has a User record for our accountid so
				// the runtime's GetUserInfo() returns non-null (legacy
				// Build called this; it lazily creates a retrieving User).
				world.lobby.GetUserInfo(world.lobby.accountid);
				SetRuntime(MISSIONSUMMARY);
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
			}
		}break;
		case SINGLEPLAYERGAME: TickSinglePlayerGame(); break;
		case OPTIONS:{
			if(stateisnew){
				world.DestroyAllObjects();
				// OptionsRuntime owns the surface. Palette + camera inherit
				// from MAINMENU; re-set defensively in case OPTIONS is
				// entered from a non-MainMenu path.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(OPTIONS);
				stateisnew = false;
			}
		}break;
		case OPTIONSCONTROLS:{
			if(stateisnew){
				world.DestroyAllObjects();
				// OptionsControlsRuntime owns the surface, click dispatch,
				// and rebind capture state machine. Palette + camera mirror
				// the Options router.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(OPTIONSCONTROLS);
				stateisnew = false;
			}
			if(active_runtime) active_runtime->Tick();
		}break;
		case OPTIONSDISPLAY:{
			if(stateisnew){
				world.DestroyAllObjects();
				// OptionsDisplayRuntime owns the surface. Palette + camera
				// mirror the Options router.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(OPTIONSDISPLAY);
				stateisnew = false;
			}
		}break;
		case OPTIONSAUDIO:{
			if(stateisnew){
				world.DestroyAllObjects();
				// OptionsAudioRuntime owns the surface. Palette + camera
				// mirror the Options router.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				SetRuntime(OPTIONSAUDIO);
				stateisnew = false;
			}
		}break;
		case HOSTGAME: TickHostGame(); break;
		case JOINGAME: TickJoinGame(); break;
		case TESTGAME: TickTestGame(); break;
		case REPLAYGAME: TickReplayGame(); break;
	}
	if(fade_i < 16 && state != FADEOUT){
		// Fade IN the palette
		SDL_Color * fadedpalette = renderer.palette.CopyWithBrightness(renderer.palette.GetColors(), (fade_i) * 8);
		if(fade_i == 15){
			SetColors(renderer.palette.GetColors());
		}else{
			SetColors(fadedpalette);
		}
	}
	if(!nextstateprocessed){
		nextstateprocessed = true;
		return Tick();
	}
	return true;
}


void Game::GoToState(Uint8 newstate){
	nextstate = newstate;
	state = FADEOUT;
	fade_i = 0;
	stateisnew = true;
	nextstateprocessed = false;
	// Defer the teardown so the active screen's Tick (which may have called
	// GoToState in response to a button click) can return safely before its
	// destructor runs.
	screenStackPendingTeardown = true;
}

bool Game::GoBack(void){
	Screen * top = GetTopScreen();
	if(top && top->HandleBack(screenContext)) return true;
	GoToState(MAINMENU);
	return false;
}




void Game::TickRumble(){
	if(!gamepad || world.gameplaystate != World::INGAME) return;
	Player* player = world.GetPeerPlayer(world.localpeerid);
	if(!player) return;

	// Fire: short high-frequency click
	if(player->rumbleFire){
		player->rumbleFire = false;
		SDL_RumbleGamepad(gamepad, 0, 12000, 80);
	}
	// Hit: strong punch on both motors
	if(player->rumbleHit){
		player->rumbleHit = false;
		SDL_RumbleGamepad(gamepad, 30000, 15000, 200);
	}
	// Land: low thud (left motor only)
	if(player->rumbleLand){
		player->rumbleLand = false;
		SDL_RumbleGamepad(gamepad, 18000, 0, 120);
	}
}


void Game::TickGamepadMenuNav(){
	// Only meaningful when a gamepad is connected and a menu interface is open.
	if(!gamepadstate.connected) return;
	Interface* iface = (Interface*)world.GetObjectFromId(currentinterface);
	// v2 LOBBY has no legacy Interface — nav_cursor lives on LobbyRuntime.
	bool v2_lobby = (!iface && state == LOBBY && active_runtime &&
	                 !LobbyV2ChatActive() && !LobbyV2CreateInputActive());
	if(!iface && !v2_lobby) return;

	// During rebind-wait (iface->disabled=true) the rebind capture code owns
	// all gamepad input.  Don't let nav/confirm/cancel fire as side effects.
	if(iface && iface->disabled) return;

	Uint32 now = SDL_GetTicks();

	// Helper: fire a nav key press with software repeat on held direction.
	auto tick = [&](GamepadNavDir& dir, Action action, Uint8 ascii){
		bool pressed = keymap.IsPressed(action, keystate, gamepadstate);
		if(!pressed){
			dir.held    = false;
			dir.nextfire = 0;
			return;
		}
		bool fire = false;
		if(!dir.held){
			// First frame held — fire immediately.
			dir.held     = true;
			dir.nextfire = now + GAMEPAD_NAV_DELAY_MS;
			fire = true;
		} else if(now >= dir.nextfire){
			// Repeat.
			dir.nextfire = now + GAMEPAD_NAV_REPEAT_MS;
			fire = true;
		}
		if(!fire) return;
		if(v2_lobby){
			// ascii 1/3 = LEFT/UP -> Prev region; 2/4 = RIGHT/DOWN -> Next.
			if(ascii == 1 || ascii == 3) LobbyV2NavPrev();
			else if(ascii == 2 || ascii == 4) LobbyV2NavNext();
		} else {
			iface->ProcessKeyPress(world, ascii);
		}
	};

	tick(gamepadNavUp,    Action::UiUp,    3);
	tick(gamepadNavDown,  Action::UiDown,  4);
	tick(gamepadNavLeft,  Action::UiLeft,  1);
	tick(gamepadNavRight, Action::UiRight, 2);

	// Confirm (A/Cross) — no repeat, edge-detect only.
	// v2 lobby has no per-region click target wired yet (P16g-2..7 wire
	// region-internal nav); skip confirm here so a stray A button doesn't
	// no-op-but-look-broken. Legacy iface path is unchanged.
	if(!iface) return;
	{
		bool confirmNow = keymap.IsPressed(Action::UiConfirm, keystate, gamepadstate);
		static bool confirmPrev = false;
		if(confirmNow && !confirmPrev){
			if(iface->activeobject == 0 && !iface->tabobjects.empty()){
				iface->ProcessKeyPress(world, 4);  // focus first item; next A confirms
			} else {
				iface->ProcessKeyPress(world, '\n');
			}
		}
		confirmPrev = confirmNow;
	}
}

const char * Game::GetActionKeyDisplayName(Action a){
	static thread_local char buf[32];
	const auto& ab = keymap.Get(a);
	// Prefer keyboard binding; fall back to any other device (gamepad/mouse).
	const BindingKey* fallback = nullptr;
	for(const auto& b : ab.bindings){
		if(b.keys.empty()) continue;
		const auto& k = b.keys[0];
		if(k.device == BindingDevice::Keyboard){
			return KeyMap::GetKeyName((SDL_Scancode)k.code);
		}
		if(!fallback) fallback = &k;
	}
	if(fallback){
		std::string s = Stringify(*fallback);
		auto colon = s.find(':');
		std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
		std::string label = GamepadShortLabel(raw, gamepad ? SDL_GetGamepadType(gamepad) : SDL_GAMEPAD_TYPE_UNKNOWN);
		std::strncpy(buf, label.c_str(), sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		return buf;
	}
	std::strncpy(buf, "(unbound)", sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	return buf;
}



const char* Game::StateName(Uint8 s){
	switch(s){
		case NONE: return "NONE";
		case FADEOUT: return "FADEOUT";
		case MAINMENU: return "MAINMENU";
		case LOBBYCONNECT: return "LOBBYCONNECT";
		case LOBBY: return "LOBBY";
		case UPDATING: return "UPDATING";
		case INGAME: return "INGAME";
		case MISSIONSUMMARY: return "MISSIONSUMMARY";
		case SINGLEPLAYERGAME: return "SINGLEPLAYERGAME";
		case OPTIONS: return "OPTIONS";
		case OPTIONSCONTROLS: return "OPTIONSCONTROLS";
		case OPTIONSDISPLAY: return "OPTIONSDISPLAY";
		case OPTIONSAUDIO: return "OPTIONSAUDIO";
		case HOSTGAME: return "HOSTGAME";
		case JOINGAME: return "JOINGAME";
		case REPLAYGAME: return "REPLAYGAME";
		case TESTGAME: return "TESTGAME";
		default: return "UNKNOWN";
	}
}

nlohmann::json Game::GetWorldSummary(){
	nlohmann::json r;
	r["map"] = world.gameinfo.mapname;
	r["peers"] = (int)world.peercount;
	nlohmann::json players = nlohmann::json::array();
	int objcount = 0;
	for(auto* o : world.objectlist){
		++objcount;
		if(o && o->type == ObjectTypes::PLAYER){
			Player* p = (Player*)o;
			nlohmann::json pj;
			pj["id"] = p->id;
			pj["hp"] = (int)p->health;
			pj["x"] = (int)p->x;
			pj["y"] = (int)p->y;
			players.push_back(std::move(pj));
		}
	}
	r["players"] = players;
	r["objects_count"] = objcount;
	return r;
}

bool Game::IsLiveMultiplayer() const {
	return (world.peercount > 1) && (world.gameplaystate == World::INGAME);
}

void Game::PushScreen(std::unique_ptr<Screen> s){
	if(!s) return;
	s->Build(screenContext);
	screenStack.push_back(std::move(s));
	currentinterface = screenStack.back()->interfaceId;
	if(currentinterface){
		world.GetAuthorityPeer()->controlledlist.push_back(currentinterface);
	}
}

void Game::PopScreen(){
	if(screenStack.empty()) return;
	screenStack.back()->Destroy(screenContext);
	screenStack.pop_back();
	currentinterface = screenStack.empty() ? 0 : screenStack.back()->interfaceId;
}

void Game::ReplaceScreen(std::unique_ptr<Screen> s){
	PopScreen();
	PushScreen(std::move(s));
}

Screen * Game::GetTopScreen() const {
	return screenStack.empty() ? nullptr : screenStack.back().get();
}

// Thin wrappers forwarding to the active LobbyRuntime. Safe to call any
// time; if the engine isn't currently in LOBBY (or the runtime hasn't
// been instantiated yet) they're no-ops / null returns. Inline static
// cast — when state == LOBBY the active_runtime is always a
// LobbyRuntime by construction (SetRuntime).
static ui::v2::LobbyRuntime * LobbyRT(Game * g){
	if(g->GetState() != GameState::LOBBY) return nullptr;
	ui::v2::Runtime * r = g->GetActiveRuntime();
	return r ? static_cast<ui::v2::LobbyRuntime *>(r) : nullptr;
}

const ui::v2::LobbyState * Game::GetLobbyV2State() const {
	if(state != LOBBY || !active_runtime) return nullptr;
	return &static_cast<ui::v2::LobbyRuntime *>(active_runtime.get())->GetState();
}

bool Game::LobbyV2ChatActive() const {
	if(state != LOBBY || !active_runtime) return false;
	return static_cast<ui::v2::LobbyRuntime *>(active_runtime.get())->ChatActive();
}

void Game::LobbyV2ChatAppendChar(char c){ if(auto * r = LobbyRT(this)) r->ChatAppendChar(c); }
void Game::LobbyV2ChatBackspace()       { if(auto * r = LobbyRT(this)) r->ChatBackspace();   }
void Game::LobbyV2ChatSubmit()          { if(auto * r = LobbyRT(this)) r->ChatSubmit();      }
bool Game::LobbyV2CreateInputActive() const {
	if(state != LOBBY || !active_runtime) return false;
	return static_cast<ui::v2::LobbyRuntime *>(active_runtime.get())->CreateInputActive();
}
void Game::LobbyV2CreateAppendChar(char c){ if(auto * r = LobbyRT(this)) r->CreateAppendChar(c); }
void Game::LobbyV2CreateBackspace()       { if(auto * r = LobbyRT(this)) r->CreateBackspace();   }
void Game::LobbyV2CreateSubmit()          { if(auto * r = LobbyRT(this)) r->CreateSubmit();      }
void Game::LobbyV2NavPrev()               { if(auto * r = LobbyRT(this)) r->NavPrev();           }
void Game::LobbyV2NavNext()               { if(auto * r = LobbyRT(this)) r->NavNext();           }

void Game::SetRuntime(Uint8 new_state){
	switch(new_state){
		case MAINMENU:
			active_runtime = std::make_unique<ui::v2::MainMenuRuntime>(world, screenContext);
			break;
		case OPTIONS:
			active_runtime = std::make_unique<ui::v2::OptionsRuntime>(world, screenContext);
			break;
		case OPTIONSDISPLAY:
			active_runtime = std::make_unique<ui::v2::OptionsDisplayRuntime>(world, screenContext);
			break;
		case OPTIONSAUDIO:
			active_runtime = std::make_unique<ui::v2::OptionsAudioRuntime>(world, screenContext);
			break;
		case OPTIONSCONTROLS:
			active_runtime = std::make_unique<ui::v2::OptionsControlsRuntime>(world, screenContext);
			break;
		case UPDATING:
			active_runtime = std::make_unique<ui::v2::UpdateRuntime>(world, screenContext);
			break;
		case MISSIONSUMMARY:
			active_runtime = std::make_unique<ui::v2::MissionSummaryRuntime>(world, screenContext);
			break;
		case LOBBYCONNECT:
			active_runtime = std::make_unique<ui::v2::LobbyConnectRuntime>(world, screenContext);
			break;
		case LOBBY:
			active_runtime = std::make_unique<ui::v2::LobbyRuntime>(world, screenContext, *this);
			break;
		default:
			active_runtime.reset();
			break;
	}
	ui_v2_last_ticks = 0;
}


// v2 modal stack — thin wrappers around the ui::v2::ModalStack instance
// owned below. Public so external callers (screen_context, lobby panels,
// runtimes) can surface a confirmation / progress / password prompt
// from any context without owning a legacy Screen subclass.
void Game::ShowV2Message(const std::string & text, std::function<void()> on_close){ ui_v2_modal_stack->ShowMessage(text, std::move(on_close)); }
void Game::ShowV2ProgressMessage(const std::string & text){                          ui_v2_modal_stack->ShowProgress(text); }
void Game::ShowV2PasswordModal(std::function<void(const std::string &)> on_submit){  ui_v2_modal_stack->ShowPassword(std::move(on_submit)); }
void Game::SetV2ProgressText(const std::string & text){                              ui_v2_modal_stack->SetProgressText(text); }
void Game::PopV2Modal(){                                                              ui_v2_modal_stack->Pop(); }
bool Game::IsV2ModalActive() const         { return ui_v2_modal_stack && ui_v2_modal_stack->Active(); }
bool Game::IsV2ProgressModalActive() const { return ui_v2_modal_stack && ui_v2_modal_stack->ProgressActive(); }
void Game::RenderV2ModalOverlay(){                          ui_v2_modal_stack->RenderOverlay(screenbuffer, renderer); }
bool Game::DispatchV2ModalClick(int lx, int ly){     return ui_v2_modal_stack->DispatchClick(lx, ly); }
bool Game::DispatchV2ModalKey(int sdl_scancode){     return ui_v2_modal_stack->DispatchKey(sdl_scancode); }
bool Game::DispatchV2ModalText(char ascii){          return ui_v2_modal_stack->DispatchText(ascii); }

ui::v2::IngameChat & Game::IngameChatOverlay(){ return *ui_v2_ingame_chat; }
ui::v2::IngameBuy  & Game::IngameBuyOverlay (){ return *ui_v2_ingame_buy;  }
ui::v2::IngameTech & Game::IngameTechOverlay(){ return *ui_v2_ingame_tech; }


void Game::TickActiveScreen(){
	if(screenStack.empty()) return;
	// Tick the topmost non-overlay plus every overlay stacked above it. Modals
	// are overlays — the screen beneath continues to tick (and run its
	// per-frame state polling) while the modal is up. Input dispatch is
	// blocked by `currentinterface` already pointing at the topmost iface.
	int start = (int)screenStack.size() - 1;
	while(start > 0 && screenStack[start]->IsOverlay()) --start;
	for(size_t i = (size_t)start; i < screenStack.size(); ++i){
		screenStack[i]->Tick(screenContext);
	}
}