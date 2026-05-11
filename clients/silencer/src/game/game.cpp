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
#include "modal.h"
#include "message_modal.h"
#include "password_modal.h"
#include "options_screen.h"
#include "options_controls_screen.h"
#include "options_display_screen.h"
#include "options_audio_screen.h"
#include "lobby_screen.h"
#include "update_screen.h"
#include "updaterstage2.h"
#include "mission_summary_screen.h"
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
#include "modals/message.h"
#include "modals/password.h"
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
	ui_v2_lobby_state = std::make_unique<ui::v2::LobbyState>();
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
		if(state == MAINMENU){
			// v2 MainMenu owns the frame: declarative tree → Layout → Render.
			// Bypasses renderer.Draw entirely (no world objects to walk).
			RenderMainMenuV2();
		}else if(state == OPTIONS){
			RenderOptionsV2();
		}else if(state == OPTIONSDISPLAY){
			RenderOptionsDisplayV2();
		}else if(state == OPTIONSAUDIO){
			RenderOptionsAudioV2();
		}else if(state == OPTIONSCONTROLS){
			RenderOptionsControlsV2();
		}else if(state == UPDATING){
			RenderUpdateV2();
		}else if(state == MISSIONSUMMARY){
			RenderMissionSummaryV2();
		}else if(state == LOBBYCONNECT){
			RenderLobbyConnectV2();
		}else if(state == LOBBY){
			RenderLobbyV2();
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
	ProcessInGameInterfaces();
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
				// v2 MainMenu owns its own rendering + click dispatch
				// (RenderMainMenuV2 / DispatchMainMenuV2Click); there's
				// no Screen on the stack, no Interface object, and no
				// currentinterface routing. Palette + camera setup mirrors
				// what MainMenuScreen::Build used to do.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				// Click handling runs in DispatchMainMenuV2Click, fired from
				// events.cpp on the SDL mouse-down edge while state==MAINMENU.
			}
		}break;
		case LOBBYCONNECT:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				world.lobby.ClearGames();
				world.lobby.state = Lobby::WAITING;
				// v2 LobbyConnect — no PushScreen. Palette 2 + camera mirror
				// LobbyConnectScreen::Build's ResetPresentation(2). Input
				// state + textbox lines reset on entry.
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				lobby_connect_username[0] = '\0';
				lobby_connect_password[0] = '\0';
				lobby_connect_active_field = 1;
				lobby_connect_textbox_lines.clear();
				lobby_connect_motdprinted = false;
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				TickLobbyConnectV2();
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
				// v2 LobbyScreen — no PushScreen. Palette 2 + camera
				// (320, 240) mirror LobbyScreen::Build's ResetPresentation(2)
				// + camera.SetPosition. Default panel is GameSelect (the
				// legacy Build creates gameSelect on construction).
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				*ui_v2_lobby_state = ui::v2::LobbyState{};
				ui_v2_lobby_state->active_panel = ui::v2::LobbyActivePanel::GameSelect;
				ui_v2_lobby_state->character.selected_agency = Config::GetInstance().defaultagency;
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				TickLobbyV2();
			}
		}break;
		case UPDATING:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				// v2 UpdateScreen — no PushScreen. Palette 2 mirrors the
				// legacy ResetPresentation(2) call in UpdateScreen::Build.
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				// STAGING -> UpdaterStage2::Launch transition (legacy
				// UpdateScreen::Tick owned this).
				TickUpdateV2();
			}
		}break;
		case INGAME: TickInGame(); break;
		case MISSIONSUMMARY:{
			if(stateisnew){
				UnloadGame();
				world.Disconnect();
				// v2 MissionSummary — no PushScreen. Palette 1 + camera
				// (320, 240) mirror MissionSummaryScreen::Build.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				// Ensure the lobby has a User record for our accountid so
				// CurrentMissionSummaryState's GetUserInfo() returns non-null
				// (legacy Build called this; it lazily creates a retrieving
				// User the first time around).
				world.lobby.GetUserInfo(world.lobby.accountid);
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
				// v2 OptionsScreen — no Interface on the stack, no
				// PushScreen. Render + click dispatch live in
				// RenderOptionsV2 / DispatchOptionsV2Click. Palette + camera
				// inherit from MAINMENU; re-set defensively in case OPTIONS
				// is entered from a non-MainMenu path.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				stateisnew = false;
			}
		}break;
		case OPTIONSCONTROLS:{
			if(stateisnew){
				world.DestroyAllObjects();
				// v2 OptionsControls — no Interface on the stack, no
				// PushScreen. Render + click dispatch + rebind capture live
				// in RenderOptionsControlsV2 / DispatchOptionsControlsV2Click
				// / TickOptionsControlsV2. Palette + camera mirror the
				// Options router.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				controls_rebind_active_slot = -1;
				controls_rebind_pending_scancode = -1;
				stateisnew = false;
			}
			TickOptionsControlsV2();
		}break;
		case OPTIONSDISPLAY:{
			if(stateisnew){
				world.DestroyAllObjects();
				// v2 OptionsDisplay — no Interface on the stack, no PushScreen.
				// Render + click dispatch live in RenderOptionsDisplayV2 /
				// DispatchOptionsDisplayV2Click. Palette + camera mirror the
				// Options router.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
				stateisnew = false;
			}
		}break;
		case OPTIONSAUDIO:{
			if(stateisnew){
				world.DestroyAllObjects();
				// v2 OptionsAudio — no Interface on the stack, no PushScreen.
				// Render + click dispatch live in RenderOptionsAudioV2 /
				// DispatchOptionsAudioV2Click. Palette + camera mirror the
				// Options router.
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				renderer.camera.SetPosition(320, 240);
				currentinterface = 0;
				ui_v2_state = ui::v2::UIState{};
				ui_v2_last_ticks = 0;
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
	if(!iface) return;

	// During rebind-wait (iface->disabled=true) the rebind capture code owns
	// all gamepad input.  Don't let nav/confirm/cancel fire as side effects.
	if(iface->disabled) return;

	Uint32 now = SDL_GetTicks();

	// Helper: fire a nav key press with software repeat on held direction.
	auto tick = [&](GamepadNavDir& dir, Action action, Uint8 ascii){
		bool pressed = keymap.IsPressed(action, keystate, gamepadstate);
		if(!pressed){
			dir.held    = false;
			dir.nextfire = 0;
			return;
		}
		if(!dir.held){
			// First frame held — fire immediately.
			dir.held     = true;
			dir.nextfire = now + GAMEPAD_NAV_DELAY_MS;
			iface->ProcessKeyPress(world, ascii);
		} else if(now >= dir.nextfire){
			// Repeat.
			dir.nextfire = now + GAMEPAD_NAV_REPEAT_MS;
			iface->ProcessKeyPress(world, ascii);
		}
	};

	tick(gamepadNavUp,    Action::UiUp,    3);
	tick(gamepadNavDown,  Action::UiDown,  4);
	tick(gamepadNavLeft,  Action::UiLeft,  1);
	tick(gamepadNavRight, Action::UiRight, 2);

	// Confirm (A/Cross) — no repeat, edge-detect only.
	// If nothing is focused, auto-focus the first item so the user sees where
	// they are before committing.
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

static ui::v2::MainMenuHandlers BuildMainMenuHandlers(Game * self, ScreenContext & sctx){
	ui::v2::MainMenuHandlers h;
	(void)self;
	// Bound to real state transitions matching the legacy MainMenuScreen::Tick
	// switch (BTN_TUTORIAL→SINGLEPLAYERGAME, BTN_LOBBY→LOBBYCONNECT,
	// BTN_OPTIONS→OPTIONS, BTN_EXIT→quit). Reaches GoToState/quit via the
	// ScreenContext we already own — keeps the surface identical to legacy.
	h.on_tutorial = [&sctx](){ sctx.GoToState(GameState::SINGLEPLAYERGAME); };
	h.on_lobby    = [&sctx](){ sctx.GoToState(GameState::LOBBYCONNECT); };
	h.on_options  = [&sctx](){ sctx.GoToState(GameState::OPTIONS); };
	h.on_exit     = [&sctx](){ sctx.RequestQuit(); };
	return h;
}

bool Game::RenderMainMenuV2(){
	// Measure dt from wallclock so the hot_t exponential approach behaves
	// the same on faster/slower machines. Matches preview.cpp's loop.
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::MainMenuHandlers handlers = BuildMainMenuHandlers(this, screenContext);
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildMainMenu(ctx, handlers);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

static ui::v2::OptionsHandlers BuildOptionsHandlers(ScreenContext & sctx){
	ui::v2::OptionsHandlers h;
	h.on_controls = [&sctx](){ sctx.GoToState(GameState::OPTIONSCONTROLS); };
	h.on_display  = [&sctx](){ sctx.GoToState(GameState::OPTIONSDISPLAY); };
	h.on_audio    = [&sctx](){ sctx.GoToState(GameState::OPTIONSAUDIO); };
	h.on_go_back  = [&sctx](){ sctx.GoToState(GameState::MAINMENU); };
	return h;
}

bool Game::RenderOptionsV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::OptionsHandlers handlers = BuildOptionsHandlers(screenContext);
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildOptions(ctx, handlers);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchOptionsV2Click(int logical_x, int logical_y){
	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::OptionsHandlers handlers = BuildOptionsHandlers(screenContext);
	ui::v2::Node tree = ui::v2::BuildOptions(ctx, handlers);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

static ui::v2::OptionsDisplayHandlers BuildOptionsDisplayHandlers(ScreenContext & sctx){
	ui::v2::OptionsDisplayHandlers h;
	h.on_toggle_fullscreen = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.fullscreen = !cfg.fullscreen;
		if(sctx.window) SDL_SetWindowFullscreen(sctx.window, cfg.fullscreen);
	};
	h.on_toggle_smooth_scaling = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.scalefilter = !cfg.scalefilter;
		if(sctx.renderdevice) sctx.renderdevice->SetScaleFilter(cfg.scalefilter);
	};
	h.on_save = [&sctx](){
		Config::GetInstance().Save();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_cancel = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.Load();
		if(sctx.renderdevice) sctx.renderdevice->SetScaleFilter(cfg.scalefilter);
		if(sctx.window) SDL_SetWindowFullscreen(sctx.window, cfg.fullscreen);
		sctx.GoToState(GameState::OPTIONS);
	};
	return h;
}

static ui::v2::OptionsDisplayState CurrentOptionsDisplayState(){
	ui::v2::OptionsDisplayState s;
	Config & cfg = Config::GetInstance();
	s.fullscreen  = cfg.fullscreen;
	s.scalefilter = cfg.scalefilter;
	return s;
}

bool Game::RenderOptionsDisplayV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::OptionsDisplayHandlers handlers = BuildOptionsDisplayHandlers(screenContext);
	ui::v2::OptionsDisplayState live = CurrentOptionsDisplayState();
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildOptionsDisplay(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchOptionsDisplayV2Click(int logical_x, int logical_y){
	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::OptionsDisplayHandlers handlers = BuildOptionsDisplayHandlers(screenContext);
	ui::v2::OptionsDisplayState live = CurrentOptionsDisplayState();
	ui::v2::Node tree = ui::v2::BuildOptionsDisplay(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

static ui::v2::OptionsAudioHandlers BuildOptionsAudioHandlers(ScreenContext & sctx){
	ui::v2::OptionsAudioHandlers h;
	h.on_toggle_music = [](){
		Config & cfg = Config::GetInstance();
		cfg.music = !cfg.music;
		if(cfg.music){
			Audio::GetInstance().ResumeMusic();
		}else{
			Audio::GetInstance().PauseMusic();
		}
	};
	h.on_save = [&sctx](){
		Config::GetInstance().Save();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_cancel = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.Load();
		if(cfg.music){
			Audio::GetInstance().ResumeMusic();
		}else{
			Audio::GetInstance().PauseMusic();
		}
		sctx.GoToState(GameState::OPTIONS);
	};
	return h;
}

static ui::v2::OptionsAudioState CurrentOptionsAudioState(){
	ui::v2::OptionsAudioState s;
	s.music = Config::GetInstance().music;
	return s;
}

bool Game::RenderOptionsAudioV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::OptionsAudioHandlers handlers = BuildOptionsAudioHandlers(screenContext);
	ui::v2::OptionsAudioState live = CurrentOptionsAudioState();
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildOptionsAudio(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchOptionsAudioV2Click(int logical_x, int logical_y){
	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::OptionsAudioHandlers handlers = BuildOptionsAudioHandlers(screenContext);
	ui::v2::OptionsAudioState live = CurrentOptionsAudioState();
	ui::v2::Node tree = ui::v2::BuildOptionsAudio(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

static ui::v2::UpdateHandlers BuildUpdateHandlers(ScreenContext & sctx, Updater & updater){
	ui::v2::UpdateHandlers h;
	h.on_update = [&updater](){
		if(updater.GetState() == Updater::PROMPTING){
			updater.Consent();
		}
	};
	h.on_cancel = [&sctx, &updater](){
		Updater::State us = updater.GetState();
		if(us == Updater::PROMPTING || us == Updater::DOWNLOADING || us == Updater::FAILED){
			if(us == Updater::DOWNLOADING){
				updater.Cancel();
			}
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	h.on_retry = [&updater](){
		if(updater.GetState() == Updater::FAILED && updater.GetRetryCount() < 3){
			updater.Retry();
		}
	};
	h.on_download = [&sctx, &updater](){
		if(updater.GetState() == Updater::FAILED && updater.GetRetryCount() >= 3){
			std::string url = updater.GetDownloadURL();
#ifdef _WIN32
			std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
			std::string cmd = "open '" + url + "'";
#else
			std::string cmd = "xdg-open '" + url + "' &";
#endif
			system(cmd.c_str());
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	return h;
}

static ui::v2::UpdateState CurrentUpdateState(Updater & updater){
	ui::v2::UpdateState s;
	Updater::State us = updater.GetState();
	switch(us){
		case Updater::PROMPTING:
			s.left = ui::v2::UpdateState::LeftButton::Update;
			s.show_cancel = true;
			s.status_text = "An update is required to play online.";
		break;
		case Updater::DOWNLOADING:{
			s.left = ui::v2::UpdateState::LeftButton::None;
			s.show_cancel = true;
			char buf[32];
			snprintf(buf, sizeof(buf), "%d%%", int(updater.GetProgress() * 100));
			s.status_text = buf;
			int width = int(updater.GetProgress() * 20.0f);
			std::string bar = "[";
			for(int i = 0; i < 20; i++){
				bar += (i < width) ? "=" : " ";
			}
			bar += "]";
			s.progress_text = bar;
		}break;
		case Updater::VERIFYING:
			s.left = ui::v2::UpdateState::LeftButton::None;
			s.show_cancel = true;
			s.status_text = "Verifying...";
		break;
		case Updater::STAGING:
			s.left = ui::v2::UpdateState::LeftButton::None;
			s.show_cancel = true;
			s.status_text = "Restarting...";
		break;
		case Updater::FAILED:{
			s.show_cancel = true;
			s.status_text = updater.GetErrorMessage();
			s.left = (updater.GetRetryCount() < 3)
				? ui::v2::UpdateState::LeftButton::Retry
				: ui::v2::UpdateState::LeftButton::Download;
		}break;
		case Updater::IDLE:
		case Updater::DONE:
		default:
			s.left = ui::v2::UpdateState::LeftButton::None;
			s.show_cancel = true;
		break;
	}
	return s;
}

bool Game::RenderUpdateV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::UpdateHandlers handlers = BuildUpdateHandlers(screenContext, updater);
	ui::v2::UpdateState live = CurrentUpdateState(updater);
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildUpdate(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchUpdateV2Click(int logical_x, int logical_y){
	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::UpdateHandlers handlers = BuildUpdateHandlers(screenContext, updater);
	ui::v2::UpdateState live = CurrentUpdateState(updater);
	ui::v2::Node tree = ui::v2::BuildUpdate(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

void Game::TickUpdateV2(){
	// Mirror UpdateScreen::Tick's STAGING branch: on STAGING, spawn the
	// stage-2 child; on success flag the Updater so Game::Loop returns
	// false next tick and ~Game tears down SDL/audio cleanly before the
	// new process opens the device (avoids audible pop).
	if(updater.GetState() != Updater::STAGING) return;
	std::string zippath =
#ifdef _WIN32
		std::string(getenv("TEMP") ? getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
		"/tmp/silencer-update.zip";
#endif
	fprintf(stderr, "[updater] Game::TickUpdateV2 invoking UpdaterStage2::Launch with zip=%s\n",
		zippath.c_str());
	if(UpdaterStage2::Launch(zippath)){
		updater.MarkStage2Spawned();
		return;
	}
	fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
	screenContext.GoToState(GameState::MAINMENU);
}

static ui::v2::MissionSummaryHandlers BuildMissionSummaryHandlers(ScreenContext & sctx){
	ui::v2::MissionSummaryHandlers h;
	h.on_done = [&sctx](){
		World & w = sctx.world;
		if(w.lobby.state == Lobby::AUTHENTICATED){
			sctx.GoToState(GameState::LOBBY);
			w.lobby.JoinChannel(w.lobby.lastchannel);
		}else{
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	h.on_upgrade = [&sctx](int slot){
		World & w = sctx.world;
		User * user = w.lobby.GetUserInfo(w.lobby.accountid);
		if(!user) return;
		w.lobby.UpgradeStat(user->statsagency, (Uint8)slot);
	};
	return h;
}

// Mirrors MissionSummaryScreen::Refresh — builds the live state from
// world.lobby.GetUserInfo(). Returns false when user info hasn't arrived
// yet; caller should fall back to a null State (matches legacy's
// pre-Refresh appearance: build-time defaults, no banner, no upgrade
// buttons). The legacy build path also clears statupgraded after each
// successful Refresh; we do that here.
static bool CurrentMissionSummaryState(World & world, ui::v2::MissionSummaryState & out){
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user) return false;
	Stats & st = user->statscopy;
	out.shaped       = st.shapedthrown;
	out.flare        = st.flaresthrown;
	out.poison_flare = st.poisonflaresthrown;
	out.neutron      = st.neutronsthrown;
	for(int i = 0; i < 4; i++){
		out.weapon_fires[i] = st.weaponfires[i];
		out.weapon_hits[i]  = st.weaponhits[i];
		out.weapon_kills[i] = st.playerkillsweapon[i];
	}
	out.xp = (int)st.CalculateExperience();
	auto & ag = user->agency[user->statsagency];
	out.levels[0] = ag.endurance;
	out.levels[1] = ag.shield;
	out.levels[2] = ag.jetpack;
	out.levels[3] = ag.techslots;
	out.levels[4] = ag.hacking;
	out.levels[5] = ag.contacts;
	if(user->retrieving){
		out.show_banner = false;
		for(int i = 0; i < 6; i++) out.show_upgrade[i] = false;
		return true;
	}
	int totalbonusupgrades = ag.endurance + ag.shield + ag.jetpack + ag.techslots + ag.hacking + ag.contacts;
	bool slot_room[6] = {
		ag.endurance < ag.maxendurance,
		ag.shield    < ag.maxshield,
		ag.jetpack   < ag.maxjetpack,
		ag.techslots < ag.maxtechslots,
		ag.hacking   < ag.maxhacking,
		ag.contacts  < ag.maxcontacts,
	};
	int maxupgrades = ag.level;
	int cap = user->TotalUpgradePointsPossible(user->statsagency);
	if(maxupgrades > cap) maxupgrades = cap;
	bool upgradeavailable = (totalbonusupgrades - ag.defaultbonuses) < maxupgrades;
	out.show_banner = upgradeavailable;
	for(int i = 0; i < 6; i++) out.show_upgrade[i] = upgradeavailable && slot_room[i];
	// Consume the upgrade-ack flag — legacy MissionSummaryScreen::Tick
	// cleared this after each Refresh; no other consumer reads it.
	world.lobby.statupgraded = false;
	return true;
}

bool Game::RenderMissionSummaryV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::MissionSummaryHandlers handlers = BuildMissionSummaryHandlers(screenContext);
	ui::v2::MissionSummaryState live;
	bool have_state = CurrentMissionSummaryState(world, live);
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildMissionSummary(ctx, handlers, have_state ? &live : nullptr);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchMissionSummaryV2Click(int logical_x, int logical_y){
	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::MissionSummaryHandlers handlers = BuildMissionSummaryHandlers(screenContext);
	ui::v2::MissionSummaryState live;
	bool have_state = CurrentMissionSummaryState(world, live);
	ui::v2::Node tree = ui::v2::BuildMissionSummary(ctx, handlers, have_state ? &live : nullptr);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

static ui::v2::LobbyConnectHandlers BuildLobbyConnectHandlers(Game * self){
	ui::v2::LobbyConnectHandlers h;
	h.on_login  = [self](){ self->LobbyConnectSubmit(); };
	h.on_cancel = [self](){ self->LobbyConnectCancel(); };
	return h;
}

void Game::LobbyConnectCancel(){
	screenContext.GoToState(GameState::MAINMENU);
}

void Game::LobbyConnectSubmit(){
	// Mirrors LobbyConnectScreen::Tick's LBY_BTN_LOGIN handler.
	if(world.lobby.state == Lobby::AUTHENTICATING){
		world.lobby.SetLocalUsername(lobby_connect_username);
		world.lobby.SendCredentials(lobby_connect_username, lobby_connect_password);
		world.lobby.state = Lobby::AUTHSENT;
	}
}

void Game::LobbyConnectCycleField(){
	lobby_connect_active_field = (lobby_connect_active_field == 1) ? 2 : 1;
}

void Game::LobbyConnectAppendChar(char c){
	if(world.lobby.state == Lobby::AUTHSENT) return;
	if(lobby_connect_active_field == 1){
		size_t len = strlen(lobby_connect_username);
		if(len < sizeof(lobby_connect_username) - 1){
			lobby_connect_username[len]   = c;
			lobby_connect_username[len+1] = '\0';
		}
	}else if(lobby_connect_active_field == 2){
		size_t len = strlen(lobby_connect_password);
		if(len < sizeof(lobby_connect_password) - 1){
			lobby_connect_password[len]   = c;
			lobby_connect_password[len+1] = '\0';
		}
	}
}

void Game::LobbyConnectBackspace(){
	if(world.lobby.state == Lobby::AUTHSENT) return;
	if(lobby_connect_active_field == 1){
		size_t len = strlen(lobby_connect_username);
		if(len > 0) lobby_connect_username[len-1] = '\0';
	}else if(lobby_connect_active_field == 2){
		size_t len = strlen(lobby_connect_password);
		if(len > 0) lobby_connect_password[len-1] = '\0';
	}
}

static ui::v2::LobbyConnectState CurrentLobbyConnectState(const Game & g,
	const char * username, const char * password, int active_field,
	const std::vector<std::string> & lines, int frames){
	ui::v2::LobbyConnectState s;
	s.username     = username;
	s.password     = password;
	s.active_field = active_field;
	// AUTHSENT = "Sending credentials" — legacy disables both TextInputs while
	// waiting on the server's response (effectbrightness=64 in DrawTextInput).
	s.inactive     = (g.GetWorldConst().lobby.state == Lobby::AUTHSENT);
	// 24 fps legacy blink: state_i % 32 < 16. Use Game::frames since it ticks
	// at the same rate.
	s.caret_visible = ((frames & 31) < 16);
	s.textbox_lines = lines;
	return s;
}

bool Game::RenderLobbyConnectV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::LobbyConnectHandlers handlers = BuildLobbyConnectHandlers(this);
	ui::v2::LobbyConnectState live = CurrentLobbyConnectState(*this,
		lobby_connect_username, lobby_connect_password, lobby_connect_active_field,
		lobby_connect_textbox_lines, frames);
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildLobbyConnect(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchLobbyConnectV2Click(int logical_x, int logical_y){
	// Hit-test the username/password input rects directly to swap active field
	// — Buttons handle Login/Cancel via the v2 dispatch pass.
	// Username box: x=[275, 275+180), y=[293, 293+14)
	// Password box: x=[275, 275+180), y=[320, 320+14)
	if(world.lobby.state != Lobby::AUTHSENT){
		if(logical_x >= 275 && logical_x < 275 + 180){
			if(logical_y >= 293 && logical_y < 293 + 14){
				lobby_connect_active_field = 1;
			}else if(logical_y >= 320 && logical_y < 320 + 14){
				lobby_connect_active_field = 2;
			}
		}
	}
	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::LobbyConnectHandlers handlers = BuildLobbyConnectHandlers(this);
	ui::v2::LobbyConnectState live = CurrentLobbyConnectState(*this,
		lobby_connect_username, lobby_connect_password, lobby_connect_active_field,
		lobby_connect_textbox_lines, frames);
	ui::v2::Node tree = ui::v2::BuildLobbyConnect(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

void Game::TickLobbyConnectV2(){
	// Mirrors LobbyConnectScreen::Tick. Gate on ambience-fade-in so the
	// "Connecting to ..." line doesn't print before the music crossfade lands.
	if(!ambienceMixer.FadedIn()) return;
	auto push_line = [this](const char * s){ lobby_connect_textbox_lines.emplace_back(s); };
	world.lobby.LockMutex();
	switch(world.lobby.state){
		case Lobby::CONNECTING:
		case Lobby::WAITINGFORRESOLVER:
		case Lobby::AUTHSENT:
		case Lobby::IDLE:
		break;
		case Lobby::WAITING:{
			char line[128];
			snprintf(line, sizeof(line), "Connecting to %s:%d", Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
			push_line(line);
			world.lobby.Connect(Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
		}break;
		case Lobby::RESOLVING:
			push_line("Resolving hostname...");
			world.lobby.state = Lobby::WAITINGFORRESOLVER;
		break;
		case Lobby::RESOLVEFAILED:
			push_line("Could not resolve hostname");
			world.lobby.state = Lobby::IDLE;
		break;
		case Lobby::RESOLVED:
			push_line("Hostname resolved");
			world.lobby.Connect(Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
		break;
		case Lobby::CONNECTED:
			push_line("Connected");
			push_line("Checking version...");
			world.lobby.SendVersion();
			world.lobby.state = Lobby::CHECKINGVERSION;
		break;
		case Lobby::CHECKINGVERSION:
			if(world.lobby.versionchecked){
				if(world.lobby.versionok){
					push_line("Software version is current");
					world.lobby.state = Lobby::AUTHENTICATING;
				}else{
					if(world.lobby.updateavailable){
						updater.PresentUpdate(world.lobby.updateurl, world.lobby.updatesha256);
						world.lobby.Disconnect();
						world.lobby.state = Lobby::IDLE;
						world.lobby.UnlockMutex();
						screenContext.GoToState(GameState::UPDATING);
						return;
					}else{
						push_line("Software is out of date");
						push_line("Get latest version at:");
						push_line("https://github.com/Arsia-Mons/Silencer");
						world.lobby.Disconnect();
						world.lobby.state = Lobby::IDLE;
					}
				}
			}
		break;
		case Lobby::AUTHENTICATING:
		break;
		case Lobby::AUTHFAILED:
			push_line("Authentication failed");
			if(strlen(world.lobby.failmessage) > 0) push_line(world.lobby.failmessage);
			world.lobby.state = Lobby::AUTHENTICATING;
		break;
		case Lobby::AUTHENTICATED:
			push_line("Authenticated");
			world.lobby.UnlockMutex();
			screenContext.GoToState(GameState::LOBBY);
			return;
		case Lobby::CONNECTIONFAILED:
			push_line("Connection failed");
			world.lobby.state = Lobby::IDLE;
		break;
		case Lobby::DISCONNECTED:
			push_line("Disconnected");
			world.lobby.state = Lobby::IDLE;
		break;
	}
	if(world.lobby.motdreceived && !lobby_connect_motdprinted){
		char * line = strtok(world.lobby.motd, "\n");
		while(line != 0){
			push_line(line);
			line = strtok(NULL, "\n");
		}
		lobby_connect_motdprinted = true;
	}
	world.lobby.UnlockMutex();
}

// Lobby v2 handlers — chrome Go Back is the only wired callback at P16g-1;
// per-panel callbacks (P16g-2..7) reuse the LobbyHandlers struct's nested
// fields. Defaulted-empty std::function entries are safe to dispatch — the
// v2 dispatch pass guards on `if(on_click)`.
static ui::v2::LobbyHandlers BuildLobbyV2Handlers(Game * game){
	ui::v2::LobbyHandlers h;
	h.on_go_back = [game](){ game->LobbyV2HandleBack(); };
	h.character.on_select_agency = [game](Uint8 agency){ game->LobbyV2SelectAgency(agency); };
	h.game_select.on_create = [game](){ game->LobbyV2ShowGameCreate(); };
	h.game_select.on_join   = [game](){ game->LobbyV2GameSelectJoin(); };
	h.game_create.on_security_toggle = [game](){ game->LobbyV2CreateCycleSecurity(); };
	h.game_create.on_create          = [game](){ game->LobbyV2CreateGame(); };
	h.game_join.on_ready             = [game](){ game->LobbyV2GameJoinReady(); };
	h.game_join.on_change_team       = [game](){ game->LobbyV2GameJoinChangeTeam(); };
	h.game_join.on_choose_tech       = [game](){ game->LobbyV2ShowGameTech(); };
	return h;
}

bool Game::LobbyV2ChatActive() const {
	using LAP = ui::v2::LobbyActivePanel;
	const LAP p = ui_v2_lobby_state->active_panel;
	return p == LAP::None || p == LAP::GameJoin || p == LAP::GameSelect;
}

// Refresh the character panel's username + LEVEL/WINS/LOSSES/XP texts from
// the live lobby state. Mirrors CharacterPanel::Tick's overlay refresh —
// safe to call every frame because the v2 builder rebuilds the tree.
void Game::RefreshLobbyV2CharacterState(){
	ui::v2::CharacterPanelState & cs = ui_v2_lobby_state->character;
	cs.username = world.lobby.GetLocalUsername();
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(user && !user->retrieving){
		Uint8 a = cs.selected_agency;
		cs.level_text  = "LEVEL: "  + std::to_string(user->agency[a].level);
		cs.wins_text   = "WINS: "   + std::to_string(user->agency[a].wins);
		cs.losses_text = "LOSSES: " + std::to_string(user->agency[a].losses);
		int lvl = user->agency[a].level;
		int remaining = 100 * (lvl + 1) - (int)user->agency[a].xptonextlevel;
		cs.xp_text = "XP TO NEXT LEVEL: " + std::to_string(remaining);
	}else{
		cs.level_text.clear();
		cs.wins_text.clear();
		cs.losses_text.clear();
		cs.xp_text.clear();
	}
}

// Mirror of ChatPanel::Tick (clients/silencer/src/ui/screens/lobby/panels/
// chat_panel.cpp). Updates the v2 chat scrollback / presence / channel
// state from world.lobby. Side-effects on world.lobby are idempotent
// (presencechanged / channelchanged latches are cleared on consume).
void Game::RefreshLobbyV2ChatState(){
	ui::v2::ChatPanelState & cs = ui_v2_lobby_state->chat;

	// Channel-name overlay.
	if(world.lobby.channelchanged){
		if(world.lobby.lastchannel[0] == '\0'){
			strcpy(world.lobby.lastchannel, world.lobby.channel);
		}
		cs.channel_name = world.lobby.channel;
		world.lobby.channelchanged = false;
	}

	// Drain incoming chat into the scrollback. Each message is a
	// vector<char> with [text\0][color][brightness] layout — see
	// Lobby's MSG_CHAT handler.
	bool any = false;
	while(!world.lobby.chatmessages.empty()){
		auto & message = world.lobby.chatmessages.front();
		const char * text = message.data();
		size_t tlen = strlen(text);
		Uint8 color = (Uint8)message[tlen + 1];
		Uint8 brightness = (Uint8)message[tlen + 2];
		// Mirror legacy TextBox::AddLine width clamp: max chars =
		// width / fontwidth = 242 / 6 = 40.
		const size_t maxchars = 242 / 6;
		std::string s(text, std::min(tlen, maxchars));
		ui::v2::ChatLine cl{std::move(s), color, brightness};
		cs.chat_lines.push_back(std::move(cl));
		// Mirror TextBox::maxlines = 256.
		if(cs.chat_lines.size() > 256){
			cs.chat_lines.pop_front();
		}
		world.lobby.chatmessages.pop_front();
		any = true;
	}
	if(any){
		// Auto-scroll to bottom (mirrors AddText(scroll=true) -> AddLine
		// scrolled = size - height/lineheight).
		const Uint16 visible = 207 / 11;
		if(cs.chat_lines.size() > visible){
			cs.chat_scrolled = (Uint16)(cs.chat_lines.size() - visible);
		}else{
			cs.chat_scrolled = 0;
		}
	}

	// Presence list — rebuild on change. Mirrors the !gamesprocessed ||
	// presencechanged trigger in ChatPanel::Tick.
	if(world.lobby.presencechanged || !world.lobby.gamesprocessed){
		cs.presence_lines.clear();
		struct Row { Uint8 group; std::string label; };
		std::vector<Row> rows;
		rows.reserve(world.lobby.presence.size());
		for(auto & kv : world.lobby.presence){
			Lobby::PresenceEntry & e = kv.second;
			Row r;
			r.label = e.name;
			r.group = (e.status <= 2) ? e.status : 0;
			if(e.gameid != 0){
				LobbyGame * g = world.lobby.GetGameById(e.gameid);
				if(g){
					r.label += " [";
					r.label += g->name;
					r.label += "]";
				}
			}
			rows.push_back(std::move(r));
		}
		std::sort(rows.begin(), rows.end(), [](const Row & a, const Row & b){
			if(a.group != b.group) return a.group < b.group;
			return a.label < b.label;
		});
		const size_t maxchars = 110 / 6;
		Uint8 lastgroup = 255;
		for(auto & r : rows){
			if(r.group != lastgroup){
				const char * header = (r.group == 0) ? "In Lobby" : (r.group == 1) ? "Pregame" : "Playing";
				ui::v2::ChatLine cl;
				cl.text = header;
				cl.color = 0;
				cl.brightness = 128 + 32;
				cs.presence_lines.push_back(std::move(cl));
				lastgroup = r.group;
			}
			ui::v2::ChatLine cl;
			cl.text = "  " + r.label;  // legacy AddText indent=2 -> "\n  "
			if(cl.text.size() > maxchars) cl.text.resize(maxchars);
			cl.color = 0;
			cl.brightness = 128;
			cs.presence_lines.push_back(std::move(cl));
		}
		world.lobby.presencechanged = false;
	}

	// Caret blink — mirror renderer.cpp:1846's `state_i % 32 < 16` gate.
	cs.caret_visible = ((Uint32)frames & 31u) < 16u;
}

// Port of GameSelectPanel::Tick (clients/silencer/src/ui/screens/lobby/
// panels/game_select_panel.cpp). Rebuilds the v2 games list from
// world.lobby.games when !gamesprocessed (mirrors legacy AddItem/DeleteItem
// reconcile), then computes the selected-game overlay snapshot.
void Game::RefreshLobbyV2GameSelectState(){
	ui::v2::GameSelectState & gs = ui_v2_lobby_state->game_select;

	// Mirror legacy "items.size() > ceil(height/lineheight)" scrollbar gate.
	const int max_visible_rows = 265 / 14;  // 18

	if(!world.lobby.gamesprocessed){
		// Drop entries whose lobby record is gone, preserving order +
		// selection of survivors.
		size_t write = 0;
		int new_selected = -1;
		Uint32 selected_id = (gs.selected_index >= 0 && gs.selected_index < (int)gs.games.size())
			? gs.games[gs.selected_index].id : 0;
		for(size_t r = 0; r < gs.games.size(); r++){
			if(world.lobby.GetGameById(gs.games[r].id)){
				if(write != r) gs.games[write] = std::move(gs.games[r]);
				if(gs.games[write].id == selected_id) new_selected = (int)write;
				write++;
			}
		}
		gs.games.resize(write);
		gs.selected_index = new_selected;

		// Append new lobby games.
		for(LobbyGame * lobbygame : world.lobby.games){
			bool found = false;
			for(auto & row : gs.games){
				if(row.id == lobbygame->id){ found = true; break; }
			}
			if(found) continue;
			ui::v2::GameSelectGame row;
			row.id = lobbygame->id;
			// Legacy SelectBox::AddItem dups via strdup(text); the
			// renderer reads it through DrawText. Match legacy without
			// the [DL] prefix (this panel never prefixes — that's the
			// game-create map list).
			row.name = lobbygame->name;
			row.passworded = (strlen(lobbygame->password) > 0);
			row.accountid = lobbygame->accountid;
			row.mapname = lobbygame->mapname;
			User * creator = world.lobby.GetUserInfo(lobbygame->accountid);
			if(creator) row.creator_name = creator->name;
			row.securitylevel = lobbygame->securitylevel;
			row.minlevel = lobbygame->minlevel;
			row.maxlevel = lobbygame->maxlevel;
			row.maxplayers = lobbygame->maxplayers;
			row.maxteams = lobbygame->maxteams;
			gs.games.push_back(std::move(row));
		}

		world.lobby.gamesprocessed = true;
	}else{
		// Keep creator_name fresh once user info arrives even if the
		// !gamesprocessed branch already ran — legacy reads through
		// GetUserInfo each Tick.
		for(auto & row : gs.games){
			if(!row.creator_name.empty()) continue;
			User * creator = world.lobby.GetUserInfo(row.accountid);
			if(creator) row.creator_name = creator->name;
		}
	}

	// Clamp selection to current list bounds.
	if(gs.selected_index >= (int)gs.games.size()) gs.selected_index = -1;

	// Show scrollbar when games overflow the visible window.
	gs.show_scrollbar = (int)gs.games.size() > max_visible_rows;
	// Clamp scroll. Without scroll input wiring this typically stays 0
	// at runtime; resilient if a future iteration plumbs scroll input.
	const int scrollmax = std::max(0, (int)gs.games.size() - max_visible_rows);
	if(gs.scrolled > scrollmax) gs.scrolled = (Uint16)scrollmax;
}

void Game::LobbyV2GameSelectRow(int index){
	ui::v2::GameSelectState & gs = ui_v2_lobby_state->game_select;
	if(index < 0 || index >= (int)gs.games.size()){
		gs.selected_index = -1;
		return;
	}
	gs.selected_index = index;
}

// Mirror of GameSelectPanel::Tick's GSEL_BTN_JOIN branch (game_select_panel.cpp:251).
void Game::LobbyV2GameSelectJoin(){
	ui::v2::GameSelectState & gs = ui_v2_lobby_state->game_select;
	if(gs.selected_index < 0 || gs.selected_index >= (int)gs.games.size()){
		ShowV2Message("No game selected");
		return;
	}
	Uint32 gameid = gs.games[gs.selected_index].id;
	if(!gameid) return;
	LobbyGame * lobbygame = world.lobby.GetGameById(gameid);
	if(!lobbygame) return;
	if(!world.IsIdle()) return;
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(user){
		if(lobbygame->minlevel > user->agency[Config::GetInstance().defaultagency].level){
			ShowV2Message("Your player level is too low");
			return;
		}
		if(lobbygame->maxlevel < user->agency[Config::GetInstance().defaultagency].level){
			ShowV2Message("Your player level is too high");
			return;
		}
	}
	currentlobbygameid = lobbygame->id;
	if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
		Game * self = this;
		Uint32 gid = lobbygame->id;
		ShowV2PasswordModal([self, gid](const std::string & password){
			LobbyGame * lg = self->GetWorld().lobby.GetGameById(gid);
			if(lg){
				char buf[64];
				std::strncpy(buf, password.c_str(), sizeof(buf) - 1);
				buf[sizeof(buf) - 1] = '\0';
				self->JoinGame(*lg, buf);
			}
		});
	}else{
		JoinGame(*lobbygame);
	}
}

void Game::LobbyV2ChatAppendChar(char c){
	ui::v2::ChatPanelState & cs = ui_v2_lobby_state->chat;
	// Mirror TextInput::ProcessKeyPress: maxchars=200 cap, scroll once
	// length exceeds maxwidth+scrolled (maxwidth=60).
	if(c < 0x20 || c > 0x7F) return;
	if(cs.chat_input.size() >= 200) return;
	if(cs.chat_input.size() >= (size_t)(60 + cs.chat_input_scrolled)){
		cs.chat_input_scrolled++;
	}
	cs.chat_input.push_back(c);
}

void Game::LobbyV2ChatBackspace(){
	ui::v2::ChatPanelState & cs = ui_v2_lobby_state->chat;
	if(cs.chat_input.empty()) return;
	cs.chat_input.pop_back();
	if(cs.chat_input_scrolled > 0) cs.chat_input_scrolled--;
}

void Game::LobbyV2ChatSubmit(){
	ui::v2::ChatPanelState & cs = ui_v2_lobby_state->chat;
	if(cs.chat_input.empty()) return;
	world.lobby.SendChat(world.lobby.channel, cs.chat_input.c_str());
	cs.chat_input.clear();
	cs.chat_input_scrolled = 0;
}

bool Game::RenderLobbyV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	RefreshLobbyV2CharacterState();
	RefreshLobbyV2ChatState();
	RefreshLobbyV2GameSelectState();
	RefreshLobbyV2GameCreateState();
	RefreshLobbyV2GameJoinState();
	ui::v2::LobbyHandlers handlers = BuildLobbyV2Handlers(this);
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildLobby(ctx, handlers, *ui_v2_lobby_state);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchLobbyV2Click(int logical_x, int logical_y){
	// Hit-test the 5 character-panel agency toggles before falling
	// through to the v2 tree dispatch. Toggles aren't a v2 NodeKind;
	// the legacy Toggle::MouseInside math reads sprite anchor + size
	// from the resources atlas — mirror it here. Bank 181, indices 0..4
	// at (20 + i*42, 90).
	for(int i = 0; i < 5; i++){
		Sint16 anchor_x = (Sint16)(20 + i * 42);
		Sint16 anchor_y = 90;
		Uint8 idx = (Uint8)i;
		int x1 = anchor_x - world.resources.spriteoffsetx[181][idx];
		int y1 = anchor_y - world.resources.spriteoffsety[181][idx];
		int x2 = x1 + world.resources.spritewidth[181][idx];
		int y2 = y1 + world.resources.spriteheight[181][idx];
		if(logical_x >= x1 && logical_x < x2 && logical_y >= y1 && logical_y < y2){
			LobbyV2SelectAgency(idx);
			return;
		}
	}

	// Game-select panel row hit-test. SelectBox occupies (407, 89,
	// 214, 265); each visible row is 14px tall starting at y=89, mapping
	// (logical_y - 89) / 14 + scrolled -> item index. Mirrors
	// SelectBox::MouseInside without the sprite-offset terms (the legacy
	// SelectBox has no res sprite, so offsets are zero).
	if(ui_v2_lobby_state->active_panel == ui::v2::LobbyActivePanel::GameSelect){
		const ui::v2::GameSelectState & gs = ui_v2_lobby_state->game_select;
		if(logical_x >= 407 && logical_x < 407 + 214 - 16 &&
		   logical_y >= 89  && logical_y < 89 + 265){
			int idx = ((logical_y - 89) / 14) + gs.scrolled;
			if(idx >= 0 && idx < (int)gs.games.size()){
				LobbyV2GameSelectRow(idx);
				return;
			}
		}
	}

	// Game-create panel hit-tests. The panel has six cycle controls
	// (security + 4 numeric cycles), two text-input focus boxes (name +
	// password), the map SelectBox (row select + [DL] badge), and the
	// Create button (handled by v2 dispatch below).
	if(ui_v2_lobby_state->active_panel == ui::v2::LobbyActivePanel::GameCreate){
		const int row_y0 = 93;
		const int row_dy = 18;
		// Security cycle button (BNONE @ x=323, w=70, h=20).
		if(logical_x >= 323 && logical_x < 323 + 70 &&
		   logical_y >= row_y0 + 0 * row_dy && logical_y < row_y0 + 0 * row_dy + 20){
			LobbyV2CreateCycleSecurity();
			return;
		}
		// Numeric cycle inputs (legacy TextInput @ x=350, w=20, h=20).
		auto numeric_row_hit = [&](int row){
			return logical_x >= 350 && logical_x < 350 + 20 &&
			       logical_y >= row_y0 + row * row_dy &&
			       logical_y < row_y0 + row * row_dy + 20;
		};
		if(numeric_row_hit(1)){ LobbyV2CreateCycleMinLevel();   return; }
		if(numeric_row_hit(2)){ LobbyV2CreateCycleMaxLevel();   return; }
		if(numeric_row_hit(3)){ LobbyV2CreateCycleMaxPlayers(); return; }
		if(numeric_row_hit(4)){ LobbyV2CreateCycleMaxTeams();   return; }
		// Map SelectBox @ (407, 89, 214, 265). Use the same row-y math as
		// game_select; [DL] badge column lives in the right 16px margin.
		if(logical_y >= 89 && logical_y < 89 + 265 &&
		   logical_x >= 407 && logical_x < 407 + 214){
			const ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
			int row = ((logical_y - 89) / 14) + gcs.map_scrolled;
			if(logical_x >= 407 + 214 - 16){
				LobbyV2CreateDownloadMap(row);
			}else{
				LobbyV2CreateSelectMap(row);
			}
			return;
		}
		// Name text-input focus box (legacy @ x=410, y=375, w=210, h=14).
		if(logical_x >= 410 && logical_x < 410 + 210 &&
		   logical_y >= 375 && logical_y < 375 + 14){
			lobby_create_active_field = 1;
			ui_v2_lobby_state->game_create.name_active = true;
			ui_v2_lobby_state->game_create.password_active = false;
			return;
		}
		// Password text-input focus box (legacy @ x=410, y=405, w=210, h=14).
		if(logical_x >= 410 && logical_x < 410 + 210 &&
		   logical_y >= 405 && logical_y < 405 + 14){
			lobby_create_active_field = 2;
			ui_v2_lobby_state->game_create.name_active = false;
			ui_v2_lobby_state->game_create.password_active = true;
			return;
		}
	}

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::LobbyHandlers handlers = BuildLobbyV2Handlers(this);
	ui::v2::Node tree = ui::v2::BuildLobby(ctx, handlers, *ui_v2_lobby_state);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

// Port of LobbyScreen::Tick (clients/silencer/src/ui/screens/lobby/
// lobby_screen.cpp:106). Panel ::Tick calls deferred to P16g-2..7 — the
// v2 panels are static at P16g-1 (preview-gate rendering only).
void Game::TickLobbyV2(){
	using LAP = ui::v2::LobbyActivePanel;
	ui::v2::LobbyState & ls = *ui_v2_lobby_state;

	// Lobby disconnect -> bounce back to the connect screen.
	if(world.lobby.state == Lobby::DISCONNECTED){
		world.Disconnect();
		screenContext.GoToState(GameState::LOBBYCONNECT);
		return;
	}

	// Deferred CreateGame state machine — kicked off by GameCreatePanel; runs
	// here so it survives the panel teardown that fires on success.
	if(ls.active_panel == LAP::GameCreate){
		int us = mapDownloader.mapUploadState.load(std::memory_order_acquire);
		if(us == 2){
			mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
			const char * pw = mapDownloader.pendingCreate.password.empty() ? nullptr : mapDownloader.pendingCreate.password.c_str();
			world.lobby.CreateGame(
				mapDownloader.pendingCreate.gamename.c_str(),
				mapDownloader.pendingCreate.mapname.c_str(),
				mapDownloader.pendingCreate.maphash,
				pw,
				mapDownloader.pendingCreate.securitylevel,
				mapDownloader.pendingCreate.minlevel,
				mapDownloader.pendingCreate.maxlevel,
				mapDownloader.pendingCreate.maxplayers,
				mapDownloader.pendingCreate.maxteams);
		}else if(us == 3){
			mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
			creategameclicked = false;
			if(IsV2ProgressModalActive()) PopV2Modal();
			ShowV2Message("Could not upload map");
		}
		if(world.lobby.creategamestatus == 1){
			world.lobby.creategamestatus = 0;
			LobbyGame * lobbygame = world.lobby.GetGameById(world.lobby.createdgameid);
			if(lobbygame){
				// Populate world.gameinfo from the lobby's record so the host can
				// SendGameInfo to the dedicated server.
				Serializer data;
				lobbygame->Serialize(Serializer::WRITE, data);
				world.gameinfo.Serialize(Serializer::READ, data);
				JoinGame(*lobbygame, lobbygame->password);
				mapDownloader.LoadMapData(mapDownloader.FindMap(lobbygame->mapname, &lobbygame->maphash).c_str());
				currentlobbygameid = lobbygame->id;
			}
		}else if(world.lobby.creategamestatus != 100 && world.lobby.creategamestatus != 0){
			world.lobby.creategamestatus = 0;
			creategameclicked = false;
			if(IsV2ProgressModalActive()) PopV2Modal();
			ShowV2Message("Could not create game");
		}
	}

	if(ls.active_panel == LAP::GameSelect || ls.active_panel == LAP::GameCreate){
		// Joining attempt finalisation (success -> CONNECTED, failure -> IDLE).
		if(joininggame){
			if(world.state == World::CONNECTED){
				joininggame = false;
			}
			if(world.state == World::IDLE){
				joininggame = false;
				if(IsV2ProgressModalActive()) PopV2Modal();
				ShowV2Message("Unable to join game");
			}
		}
		// Spinner text update for the create-game progress modal.
		if(IsV2ProgressModalActive()){
			std::string text = (mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 1)
				? "Uploading map" : "Creating game";
			int dots = (world.tickcount / 4) % 6;
			if(dots > 3) dots = 6 - dots;
			for(int i = 0; i < dots; i++) text += ".";
			SetV2ProgressText(text);
		}
		// Auto-dismiss the progress modal once the create flow has settled.
		if(IsV2ProgressModalActive() && world.lobby.creategamestatus != 100 &&
		   mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 0 &&
		   (world.state == World::CONNECTED || world.state == World::IDLE)){
			PopV2Modal();
			creategameclicked = false;
		}
		// CONNECTED transition — swap in the GameJoinPanel, refresh chat
		// channel, set the map-name overlay.
		if(world.state == World::CONNECTED){
			Peer * peer = world.peerlist[world.localpeerid];
			if(peer){
				mapDownloader.mapexistchecked = false;
				mapDownloader.mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
				mapDownloader.mapjoinstate.store(0, std::memory_order_relaxed);
				if(mapDownloader.mapjointhread.joinable()) mapDownloader.mapjointhread.detach();
				world.SetTech(Config::GetInstance().defaulttechchoices[Config::GetInstance().defaultagency]);
				LobbyV2ShowGameJoin();
				LobbyGame * lobbygame = world.lobby.GetGameById(currentlobbygameid);
				if(lobbygame){
					char temp[256];
					ambienceMixer.GetGameChannelName(*lobbygame, temp);
					strcpy(world.lobby.lastchannel, world.lobby.channel);
					world.lobby.JoinChannel(temp);
					ls.map_name = std::string(lobbygame->mapname).substr(0, 25);
				}
			}
		}
	}

	mapDownloader.ProcessMapDownload();

	// Disconnected-from-game modal: only when in the joined-game surface.
	if(world.state != World::CONNECTED && !IsV2ModalActive()){
		if(ls.active_panel == LAP::GameJoin || ls.active_panel == LAP::GameTech){
			Game * self = this;
			ShowV2Message("Disconnected from game", [self](){ self->GoBack(); });
		}
	}
}

void Game::LobbyV2HandleBack(){
	using LAP = ui::v2::LobbyActivePanel;
	ui::v2::LobbyState & ls = *ui_v2_lobby_state;
	// Joined-game surfaces -> leave the session, drop the map-name overlay,
	// swap back to the games list.
	if(ls.active_panel == LAP::GameJoin || ls.active_panel == LAP::GameTech){
		LeaveJoinedGame();
		ls.map_name.clear();
		LobbyV2ShowGameSelect();
		return;
	}
	// Create-game surface -> just swap back to games list.
	if(ls.active_panel == LAP::GameCreate){
		world.lobby.gamesprocessed = false;
		LobbyV2ShowGameSelect();
		return;
	}
	// On the games-list surface -> pop to MAINMENU.
	GoToState(MAINMENU);
}

void Game::LobbyV2ShowGameSelect(){
	// Mirror legacy: GameSelectPanel is freshly constructed on each
	// ShowGameSelect, so SelectBox::selecteditem (initialised to -1)
	// resets. Drop our selection + force a rebuild on next render.
	ui_v2_lobby_state->game_select.selected_index = -1;
	ui_v2_lobby_state->game_select.scrolled = 0;
	world.lobby.gamesprocessed = false;
	ui_v2_lobby_state->active_panel = ui::v2::LobbyActivePanel::GameSelect;
}

// Mirror of GameCreatePanel::Build (clients/silencer/src/ui/screens/lobby/
// panels/game_create_panel.cpp). Reset all per-panel state to defaults and
// rebuild the map list from local files + the community map server.
void Game::LobbyV2ShowGameCreate(){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	gcs = ui::v2::GameCreateState{};
	gcs.game_name = Config::GetInstance().defaultgamename;
	gcs.name_active     = true;
	gcs.password_active = false;
	lobby_create_active_field = 1;

	std::vector<std::string> maps;
	CDResDir();
	auto local = mapDownloader.ListFiles((GetResDir() + "level").c_str());
	maps.insert(maps.end(), local.begin(), local.end());
	CDDataDir();
	auto downloads = mapDownloader.ListFiles((GetDataDir() + "level/download").c_str());
	for(auto & d : downloads){
		if(std::find(maps.begin(), maps.end(), d) == maps.end()) maps.push_back(d);
	}
	std::sort(maps.begin(), maps.end());
	mapDownloader.servermaps.clear();
	auto serverlist = FetchServerMapList(Config::GetInstance().mapapiurl);
	for(auto & entry : serverlist){
		if(std::find(maps.begin(), maps.end(), entry.first) == maps.end()){
			std::string label = "[DL] " + entry.first;
			maps.push_back(label);
			gcs.server_maps.insert(label);
			mapDownloader.servermaps[label] = entry.second;
		}
	}
	gcs.map_items = std::move(maps);
	mapDownloader.selectedmap = -1;
	creategameclicked = false;

	ui_v2_lobby_state->active_panel = ui::v2::LobbyActivePanel::GameCreate;
}

// Per-render polling of the in-flight [DL] download + caret blink.
// Mirrors the dl-completion branch of GameCreatePanel::Tick.
void Game::RefreshLobbyV2GameCreateState(){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	gcs.caret_visible = ((Uint32)frames & 31u) < 16u;

	if(!mapDownloader.dlitemname.empty()){
		int res = mapDownloader.dlresult.load();
		if(res == 1){
			// Success — strip the [DL] prefix from the completed entry so
			// it shows as a regular local map.
			std::string completed = mapDownloader.dlitemname;
			mapDownloader.servermaps.erase(completed);
			gcs.server_maps.erase(completed);
			for(auto & item : gcs.map_items){
				if(item == completed){
					if(item.size() > 5) item = item.substr(5);
					break;
				}
			}
			gcs.download_item.clear();
			gcs.download_progress = -1;
			mapDownloader.dlitemname.clear();
		}else if(res == -1){
			gcs.download_item.clear();
			gcs.download_progress = -1;
			mapDownloader.dlitemname.clear();
			ShowV2Message("Download failed");
		}else{
			gcs.download_progress = mapDownloader.dlprogress.load();
		}
	}
}

bool Game::LobbyV2CreateInputActive() const {
	using LAP = ui::v2::LobbyActivePanel;
	return ui_v2_lobby_state->active_panel == LAP::GameCreate &&
	       lobby_create_active_field > 0;
}

void Game::LobbyV2CreateAppendChar(char c){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	if(c < 0x20 || c > 0x7F) return;
	if(lobby_create_active_field == 1){
		// Name input — legacy TextInput::maxchars = 35.
		if(gcs.game_name.size() >= 35) return;
		gcs.game_name.push_back(c);
	}else if(lobby_create_active_field == 2){
		// Password input — legacy TextInput::maxchars = 20.
		if(gcs.password.size() >= 20) return;
		gcs.password.push_back(c);
	}
}

void Game::LobbyV2CreateBackspace(){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	if(lobby_create_active_field == 1 && !gcs.game_name.empty()){
		gcs.game_name.pop_back();
	}else if(lobby_create_active_field == 2 && !gcs.password.empty()){
		gcs.password.pop_back();
	}
}

void Game::LobbyV2CreateSubmit(){
	// ENTER fires the Create button (legacy gamecreateinterface->buttonenter
	// = gamecreatebutton).
	LobbyV2CreateGame();
}

void Game::LobbyV2CreateCycleSecurity(){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	if(gcs.security_text == "Off")         gcs.security_text = "Low";
	else if(gcs.security_text == "Low")    gcs.security_text = "Medium";
	else if(gcs.security_text == "Medium") gcs.security_text = "High";
	else                                   gcs.security_text = "Off";
}

namespace {
void CycleNumeric(std::string & dst, int lo, int hi){
	int v = std::atoi(dst.c_str());
	v = (v >= hi) ? lo : (v + 1);
	dst = std::to_string(v);
}
}

void Game::LobbyV2CreateCycleMinLevel()  { CycleNumeric(ui_v2_lobby_state->game_create.min_level_text,   0,  99); }
void Game::LobbyV2CreateCycleMaxLevel()  { CycleNumeric(ui_v2_lobby_state->game_create.max_level_text,   0,  99); }
void Game::LobbyV2CreateCycleMaxPlayers(){ CycleNumeric(ui_v2_lobby_state->game_create.max_players_text, 1,  24); }
void Game::LobbyV2CreateCycleMaxTeams()  { CycleNumeric(ui_v2_lobby_state->game_create.max_teams_text,   1,  16); }

void Game::LobbyV2CreateSelectMap(int row){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	if(row < 0 || row >= (int)gcs.map_items.size()){
		gcs.selected_map = -1;
		mapDownloader.selectedmap = -1;
		return;
	}
	gcs.selected_map = row;
	mapDownloader.selectedmap = row;
}

void Game::LobbyV2CreateDownloadMap(int row){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	if(!mapDownloader.dlitemname.empty()) return;
	if(row < 0 || row >= (int)gcs.map_items.size()) return;
	const std::string & item = gcs.map_items[row];
	auto dlit = mapDownloader.servermaps.find(item);
	if(dlit == mapDownloader.servermaps.end()) return;
	const std::string & hex = dlit->second;
	if(hex.size() != 40) return;
	unsigned char sha1bytes[20] = {};
	auto hv = [](char c) -> int {
		if(c >= '0' && c <= '9') return c - '0';
		if(c >= 'a' && c <= 'f') return c - 'a' + 10;
		if(c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	};
	for(int j = 0; j < 20; j++){
		int hi = hv(hex[2*j]), lo = hv(hex[2*j+1]);
		if(hi < 0 || lo < 0) return;
		sha1bytes[j] = (unsigned char)((hi << 4) | lo);
	}
	std::string dlname = item.substr(5);
	mapDownloader.dlitemname = item;
	mapDownloader.dlresult.store(0);
	mapDownloader.dlprogress.store(0);
	gcs.download_item = dlname;
	gcs.download_progress = 0;
	if(mapDownloader.dlthread.joinable()) mapDownloader.dlthread.join();
	std::string apiURL = Config::GetInstance().mapapiurl;
	std::atomic<int> * progressPtr = &mapDownloader.dlprogress;
	std::atomic<int> * resultPtr   = &mapDownloader.dlresult;
	mapDownloader.dlthread = std::thread([dlname, sha1bytes, apiURL, progressPtr, resultPtr]() mutable {
		std::string res = FetchMapFromServer(dlname.c_str(), sha1bytes, apiURL.c_str(), progressPtr);
		resultPtr->store(res.empty() ? -1 : 1);
	});
}

// Mirror of GameCreatePanel::Tick's GCRT_BTN_CREATE branch (game_create_panel.cpp:458).
void Game::LobbyV2CreateGame(){
	ui::v2::GameCreateState & gcs = ui_v2_lobby_state->game_create;
	if(creategameclicked) return;

	if(gcs.game_name.empty()){
		ShowV2Message("No game name");
		return;
	}
	if(gcs.selected_map < 0 || gcs.selected_map >= (int)gcs.map_items.size()){
		ShowV2Message("No map selected");
		return;
	}
	const std::string & mapname = gcs.map_items[gcs.selected_map];
	if(gcs.server_maps.count(mapname) > 0){
		ShowV2Message("Download the map first");
		return;
	}

	Uint8 securitylevel = LobbyGame::SECNONE;
	if(gcs.security_text == "Low")         securitylevel = LobbyGame::SECLOW;
	else if(gcs.security_text == "Medium") securitylevel = LobbyGame::SECMEDIUM;
	else if(gcs.security_text == "High")   securitylevel = LobbyGame::SECHIGH;

	Uint8 minlevel   = (Uint8)std::atoi(gcs.min_level_text.c_str());
	Uint8 maxlevel   = (Uint8)std::atoi(gcs.max_level_text.c_str());
	Uint8 maxplayers = (Uint8)std::atoi(gcs.max_players_text.c_str());
	if(maxplayers == 0) maxplayers = 1;
	Uint8 maxteams   = (Uint8)std::atoi(gcs.max_teams_text.c_str());
	if(maxteams == 0) maxteams = 1;

	unsigned char maphash[20];
	mapDownloader.CalculateMapHash(mapDownloader.FindMap(mapname.c_str()).c_str(), &maphash);
	mapDownloader.pendingCreate.gamename = gcs.game_name;
	mapDownloader.pendingCreate.mapname  = mapname;
	mapDownloader.pendingCreate.password = gcs.password;
	memcpy(mapDownloader.pendingCreate.maphash, maphash, 20);
	mapDownloader.pendingCreate.securitylevel = securitylevel;
	mapDownloader.pendingCreate.minlevel      = minlevel;
	mapDownloader.pendingCreate.maxlevel      = maxlevel;
	mapDownloader.pendingCreate.maxplayers    = maxplayers;
	mapDownloader.pendingCreate.maxteams      = maxteams;
	if(mapDownloader.mapUploadThread.joinable()) mapDownloader.mapUploadThread.detach();
	uint32_t gen = ++mapDownloader.mapUploadGeneration;
	std::string mppath = mapDownloader.FindMap(mapname.c_str());
	std::string dataDir = GetDataDir();
	bool isBundledMap = dataDir.empty() || mppath.substr(0, dataDir.size()) != dataDir;
	std::string apiURL = Config::GetInstance().mapapiurl;
	if(isBundledMap){
		mapDownloader.mapUploadState.store(2, std::memory_order_release);
	}else{
		mapDownloader.mapUploadState.store(1, std::memory_order_relaxed);
		std::string mapname_str = mapname;
		std::atomic<int> * uploadStatePtr = &mapDownloader.mapUploadState;
		std::atomic<uint32_t> * uploadGenPtr = &mapDownloader.mapUploadGeneration;
		mapDownloader.mapUploadThread = std::thread([mapname_str, mppath, apiURL, gen, uploadStatePtr, uploadGenPtr](){
			bool ok = UploadMapToServer(mapname_str.c_str(), mppath.c_str(), apiURL.c_str());
			if(uploadGenPtr->load(std::memory_order_relaxed) != gen) return;
			uploadStatePtr->store(ok ? 2 : 3, std::memory_order_release);
		});
	}
	creategameclicked = true;
	std::strncpy(Config::GetInstance().defaultgamename, gcs.game_name.c_str(),
	             sizeof(Config::GetInstance().defaultgamename) - 1);
	Config::GetInstance().defaultgamename[sizeof(Config::GetInstance().defaultgamename) - 1] = '\0';
	Config::GetInstance().Save();
	ShowV2ProgressMessage("Uploading map...");
}

void Game::LobbyV2ShowGameJoin(){
	// Mirror of legacy TearDownRightPanels' gameTech branch: when leaving the
	// tech-choice surface, restore in-lobby team overlays. Idempotent if no
	// TEAM objects exist yet (CONNECTED-transition entry path runs before
	// teams spawn).
	if(world.choosingtech){
		world.choosingtech = false;
		ShowTeamOverlays(true);
	}
	ui_v2_lobby_state->game_join = ui::v2::GameJoinState{};
	ui_v2_lobby_state->active_panel = ui::v2::LobbyActivePanel::GameJoin;
}

// Mirrors GameJoinPanel::Tick's per-frame Ready-label derivation
// (game_join_panel.cpp:73). Only updates while world.gameplaystate ==
// INLOBBY — outside that window the legacy panel leaves the label as
// last seeded ("Ready" by Build).
void Game::RefreshLobbyV2GameJoinState(){
	if(ui_v2_lobby_state->active_panel != ui::v2::LobbyActivePanel::GameJoin) return;
	if(world.gameplaystate != World::INLOBBY) return;
	Peer * localpeer = world.peerlist[world.localpeerid];
	bool blocked = localpeer && localpeer->ishost && !world.AllPeersDownloadedMap();
	ui_v2_lobby_state->game_join.waiting = blocked;
}

// Mirror of GJN_BTN_READY branch (game_join_panel.cpp:88). Host clients
// must wait until every peer has the map; non-hosts can ready any time.
void Game::LobbyV2GameJoinReady(){
	Peer * localpeer = world.peerlist[world.localpeerid];
	bool ishost = localpeer && localpeer->ishost;
	if(!ishost || world.AllPeersDownloadedMap()){
		world.SendReady();
	}
}

void Game::LobbyV2GameJoinChangeTeam(){
	world.ChangeTeam();
}

void Game::LobbyV2ShowGameTech(){
	world.choosingtech = true;
	ShowTeamOverlays(false);
	world.RequestPeerList();
	ui_v2_lobby_state->active_panel = ui::v2::LobbyActivePanel::GameTech;
}

void Game::LobbyV2SelectAgency(Uint8 agency){
	if(agency >= 5) return;
	if(ui_v2_lobby_state->character.selected_agency == agency) return;
	ui_v2_lobby_state->character.selected_agency = agency;
	Config::GetInstance().defaultagency = agency;
	Config::GetInstance().Save();
	if(world.IsConnected()){
		world.SetAgency(agency);
	}
}

namespace {

constexpr int CONTROLS_VISIBLE_ROWS       = 5;
constexpr Uint32 CONTROLS_REBIND_TIMEOUT  = 72;
constexpr int CONTROLS_SECONDARY_SLOT_BASE = 100;

bool IsBuiltinKeybindProfile(const std::string & name){
	return name == "default" || name == "wasd" || name == "gamepad";
}

// Mirror of OptionsControlsScreen::ViewLegacy — the legacy method is
// private+static, kept here so we don't need to befriend Game.
bool BindingsAreAnded(const ActionBindings & ab){
	if(ab.bindings.empty()) return false;
	const auto & b0 = ab.bindings[0];
	return b0.keys.size() >= 2 &&
	       b0.keys[0].device == BindingDevice::Keyboard &&
	       b0.keys[1].device == BindingDevice::Keyboard;
}

std::string ControlsBindingLabel(const KeyMap & km, SDL_Gamepad * pad, Action a, int slot){
	const auto & ab = km.Get(a);
	int found = 0;
	for(const auto & b : ab.bindings){
		if(b.keys.empty()) continue;
		if(found == slot){
			const auto & k = b.keys[0];
			if(k.device == BindingDevice::Keyboard){
				return KeyMap::GetKeyName((SDL_Scancode)k.code);
			}
			std::string s = Stringify(k);
			auto colon = s.find(':');
			std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
			return GamepadShortLabel(raw, pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN);
		}
		found++;
	}
	return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
}

// Rebuild the keymap entry for one action from a primary/secondary slot
// view + the OR/AND mode (mirror of OptionsControlsScreen::WriteLegacy).
void WriteControlsLegacy(KeyMap & km, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_){
	auto & ab = km.Get(a);
	ab.bindings.clear();
	auto mk = [](SDL_Scancode sc){
		BindingKey k;
		k.device  = BindingDevice::Keyboard;
		k.code    = (int)sc;
		k.axisDir = 0;
		return k;
	};
	if(key1 == SDL_SCANCODE_UNKNOWN && key2 == SDL_SCANCODE_UNKNOWN) return;
	if(and_ && key1 != SDL_SCANCODE_UNKNOWN && key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1)); b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
		return;
	}
	if(key1 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1));
		ab.bindings.push_back(std::move(b));
	}
	if(key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
	}
}

// Existing primary/secondary scancodes for `a`. Used to keep the
// untouched slot when rebinding just one half of a pair.
void ControlsCurrentKeys(const KeyMap & km, Action a, SDL_Scancode & key1, SDL_Scancode & key2){
	key1 = key2 = SDL_SCANCODE_UNKNOWN;
	const auto & ab = km.Get(a);
	if(ab.bindings.empty()) return;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		key1 = (SDL_Scancode)b0.keys[0].code;
		key2 = (SDL_Scancode)b0.keys[1].code;
		return;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		key1 = (SDL_Scancode)b0.keys[0].code;
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			key2 = (SDL_Scancode)b1.keys[0].code;
		}
	}
}

ui::v2::OptionsControlsState ComputeOptionsControlsState(ScreenContext & sctx, int active_slot_uid){
	ui::v2::OptionsControlsState s;
	const KeyMap & km = sctx.keymap;
	SDL_Gamepad * pad = sctx.game.GetGamepad();
	s.preset_text = !km.label.empty() ? km.label
	              : !km.name.empty()  ? km.name
	              : std::string(Config::GetInstance().active_keybind_profile);
	for(int i = 0; i < CONTROLS_VISIBLE_ROWS; i++){
		int row = i;  // scroll_position currently fixed at 0
		if(row >= (int)Action::Count) break;
		Action a = ACTION_TABLE[row].action;
		s.rows[i].keyname = std::string(GetActionInfo(a).label) + ":";
		s.rows[i].c1_text = ControlsBindingLabel(km, pad, a, 0);
		s.rows[i].c2_text = ControlsBindingLabel(km, pad, a, 1);
		s.rows[i].op_text = BindingsAreAnded(km.Get(a)) ? "AND" : "OR";
		// While a slot in this row is awaiting input, override its text
		// to "-" (legacy parity).
		if(active_slot_uid >= 0){
			if(active_slot_uid < CONTROLS_SECONDARY_SLOT_BASE && active_slot_uid == row){
				s.rows[i].c1_text = "-";
			}else if(active_slot_uid >= CONTROLS_SECONDARY_SLOT_BASE &&
			         (active_slot_uid - CONTROLS_SECONDARY_SLOT_BASE) == row){
				s.rows[i].c2_text = "-";
			}
		}
	}
	return s;
}

}  // namespace

static ui::v2::OptionsControlsHandlers BuildOptionsControlsHandlers(Game * self, ScreenContext & sctx){
	ui::v2::OptionsControlsHandlers h;
	h.on_preset = [&sctx](){ CycleKeybindPreset(sctx.keymap); };
	h.on_save = [&sctx](){
		const std::string active = Config::GetInstance().active_keybind_profile;
		if(!IsBuiltinKeybindProfile(active)){
			sctx.keymap.SaveFile(WritableProfilePath(active));
		}
		Config::GetInstance().Save();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_cancel = [&sctx](){
		LoadActiveKeymap(sctx.keymap);
		Config::GetInstance().Load();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_rebind_key = [self](int row, int slot){ self->StartControlsRebind(row, slot); };
	return h;
}

void Game::StartControlsRebind(int row, int slot){
	if(controls_rebind_active_slot >= 0) return;
	controls_rebind_active_slot = (slot == 0) ? row : (CONTROLS_SECONDARY_SLOT_BASE + row);
	controls_rebind_start_tick = world.tickcount;
	controls_rebind_pending_scancode = -1;
	controls_rebind_gamepad_buttons = gamepadstate.buttons;
	std::memcpy(controls_rebind_gamepad_axes, gamepadstate.axes,
	            sizeof(controls_rebind_gamepad_axes));
}

bool Game::RenderOptionsControlsV2(){
	Uint64 now = SDL_GetTicks();
	float dt = (ui_v2_last_ticks == 0) ? 0.0f : (float)(now - ui_v2_last_ticks) / 1000.0f;
	ui_v2_last_ticks = now;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;
	ctx.dt      = dt;

	ui::v2::OptionsControlsHandlers handlers = BuildOptionsControlsHandlers(this, screenContext);
	ui::v2::OptionsControlsState live = ComputeOptionsControlsState(screenContext, controls_rebind_active_slot);
	screenbuffer.Clear(0);
	ui_v2_state.BeginFrame();
	ui::v2::Node tree = ui::v2::BuildOptionsControls(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
	ui_v2_state.EndFrame();
	return true;
}

void Game::DispatchOptionsControlsV2Click(int logical_x, int logical_y){
	// Suppress chip clicks while a rebind is in flight — legacy gates this
	// via iface->disabled; v2 mirrors by skipping dispatch entirely. Save,
	// Cancel and Preset stay reachable because the user is expected to
	// finish or time out the capture before navigating away.
	if(controls_rebind_active_slot >= 0) return;

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::OptionsControlsHandlers handlers = BuildOptionsControlsHandlers(this, screenContext);
	ui::v2::OptionsControlsState live = ComputeOptionsControlsState(screenContext, controls_rebind_active_slot);
	ui::v2::Node tree = ui::v2::BuildOptionsControls(ctx, handlers, &live);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

void Game::TickOptionsControlsV2(){
	if(controls_rebind_active_slot < 0) return;

	const int slot_uid = controls_rebind_active_slot;
	const int row = (slot_uid < CONTROLS_SECONDARY_SLOT_BASE) ? slot_uid
	                                                          : (slot_uid - CONTROLS_SECONDARY_SLOT_BASE);
	if(row < 0 || row >= (int)Action::Count){
		controls_rebind_active_slot = -1;
		controls_rebind_pending_scancode = -1;
		return;
	}
	const Action a = ACTION_TABLE[row].action;

	// Gamepad capture (only newly-pressed buttons / axes past deadzone).
	if(gamepadstate.connected){
		BindingKey padKey{}; bool padFound = false;
		for(int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && !padFound; b++){
			bool was = (controls_rebind_gamepad_buttons >> b) & 1;
			bool is  = (gamepadstate.buttons >> b) & 1;
			if(is && !was){
				padKey.device = BindingDevice::GamepadButton;
				padKey.code   = b;
				padKey.axisDir = 0;
				padFound = true;
			}
		}
		for(int ax = 0; ax < SDL_GAMEPAD_AXIS_COUNT && !padFound; ax++){
			int16_t was = controls_rebind_gamepad_axes[ax];
			int16_t is  = gamepadstate.axes[ax];
			if(std::abs(is) > AXIS_DEADZONE && std::abs(was) <= AXIS_DEADZONE){
				padKey.device  = BindingDevice::GamepadAxis;
				padKey.code    = ax;
				padKey.axisDir = (is > 0) ? 1 : -1;
				padFound = true;
			}
		}
		if(padFound){
			ForkActiveProfileIfBuiltin(keymap);
			auto & ab = keymap.Get(a);
			Binding binding; binding.keys.push_back(padKey);
			if(slot_uid < CONTROLS_SECONDARY_SLOT_BASE){
				if(ab.bindings.empty()) ab.bindings.push_back(binding);
				else                    ab.bindings[0] = binding;
			}else{
				if(ab.bindings.empty()) ab.bindings.push_back(Binding{});
				if(ab.bindings.size() < 2) ab.bindings.push_back(binding);
				else                       ab.bindings[1] = binding;
			}
			controls_rebind_active_slot = -1;
			controls_rebind_pending_scancode = -1;
			return;
		}
	}

	// Keyboard scancode or timeout.
	const bool timed_out = (world.tickcount - controls_rebind_start_tick) > CONTROLS_REBIND_TIMEOUT;
	if(controls_rebind_pending_scancode >= 0 || timed_out){
		SDL_Scancode sym = (controls_rebind_pending_scancode >= 0)
			? (SDL_Scancode)controls_rebind_pending_scancode
			: SDL_SCANCODE_UNKNOWN;
		if(timed_out) sym = SDL_SCANCODE_UNKNOWN;
#ifndef OUYA
		if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
		SDL_Scancode key1, key2;
		ControlsCurrentKeys(keymap, a, key1, key2);
		const bool and_was = BindingsAreAnded(keymap.Get(a));
		if(slot_uid < CONTROLS_SECONDARY_SLOT_BASE) key1 = sym;
		else                                        key2 = sym;
		ForkActiveProfileIfBuiltin(keymap);
		WriteControlsLegacy(keymap, a, key1, key2, and_was);
		controls_rebind_active_slot = -1;
		controls_rebind_pending_scancode = -1;
	}
}

void Game::DispatchMainMenuV2Click(int logical_x, int logical_y){
	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;
	ui::v2::MainMenuHandlers handlers = BuildMainMenuHandlers(this, screenContext);
	ui::v2::Node tree = ui::v2::BuildMainMenu(ctx, handlers);
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
}

void Game::ShowV2Message(const std::string & text, std::function<void()> on_close){
	V2ModalEntry m;
	m.kind = V2ModalEntry::MESSAGE;
	m.text = text;
	m.has_ok = true;
	m.on_close = std::move(on_close);
	ui_v2_modal_stack.push_back(std::move(m));
}

void Game::ShowV2ProgressMessage(const std::string & text){
	V2ModalEntry m;
	m.kind = V2ModalEntry::MESSAGE;
	m.text = text;
	m.has_ok = false;
	ui_v2_modal_stack.push_back(std::move(m));
}

void Game::ShowV2PasswordModal(std::function<void(const std::string &)> on_submit){
	V2ModalEntry m;
	m.kind = V2ModalEntry::PASSWORD;
	m.on_submit = std::move(on_submit);
	ui_v2_modal_stack.push_back(std::move(m));
}

void Game::SetV2ProgressText(const std::string & text){
	if(ui_v2_modal_stack.empty()) return;
	ui_v2_modal_stack.back().text = text;
}

void Game::PopV2Modal(){
	if(ui_v2_modal_stack.empty()) return;
	ui_v2_modal_stack.pop_back();
}

bool Game::IsV2ProgressModalActive() const {
	if(ui_v2_modal_stack.empty()) return false;
	const V2ModalEntry & top = ui_v2_modal_stack.back();
	return top.kind == V2ModalEntry::MESSAGE && !top.has_ok;
}

void Game::RenderV2ModalOverlay(){
	if(ui_v2_modal_stack.empty()) return;
	const V2ModalEntry & top = ui_v2_modal_stack.back();

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = ui_v2_mouse_x;
	ctx.mouse_y = ui_v2_mouse_y;
	ctx.state   = &ui_v2_state;

	ui::v2::Node tree;
	if(top.kind == V2ModalEntry::MESSAGE){
		ui::v2::MessageHandlers h;
		// The handler captures the top entry's index — pop first, then fire
		// the user callback so PopV2Modal can be called from inside it.
		h.on_ok = [this](){
			auto cb = ui_v2_modal_stack.empty() ? std::function<void()>{} : std::move(ui_v2_modal_stack.back().on_close);
			PopV2Modal();
			if(cb) cb();
		};
		tree = ui::v2::BuildMessage(ctx, top.text, top.has_ok, h);
	}else{
		ui::v2::PasswordHandlers h;
		h.on_submit = [this](const std::string & captured){
			auto cb = ui_v2_modal_stack.empty() ? std::function<void(const std::string &)>{} : std::move(ui_v2_modal_stack.back().on_submit);
			PopV2Modal();
			if(cb) cb(captured);
		};
		tree = ui::v2::BuildPassword(ctx, top.password_buf, h);
	}
	ui::v2::Layout(tree, ctx);
	ui::v2::Render(tree, ctx, screenbuffer, renderer);
}

bool Game::DispatchV2ModalClick(int logical_x, int logical_y){
	if(ui_v2_modal_stack.empty()) return false;
	const V2ModalEntry & top = ui_v2_modal_stack.back();

	ui::v2::Context ctx{
		world.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &ui_v2_state;

	ui::v2::Node tree;
	if(top.kind == V2ModalEntry::MESSAGE){
		ui::v2::MessageHandlers h;
		h.on_ok = [this](){
			auto cb = ui_v2_modal_stack.empty() ? std::function<void()>{} : std::move(ui_v2_modal_stack.back().on_close);
			PopV2Modal();
			if(cb) cb();
		};
		tree = ui::v2::BuildMessage(ctx, top.text, top.has_ok, h);
	}else{
		ui::v2::PasswordHandlers h;
		h.on_submit = [this](const std::string & captured){
			auto cb = ui_v2_modal_stack.empty() ? std::function<void(const std::string &)>{} : std::move(ui_v2_modal_stack.back().on_submit);
			PopV2Modal();
			if(cb) cb(captured);
		};
		tree = ui::v2::BuildPassword(ctx, top.password_buf, h);
	}
	ui::v2::Layout(tree, ctx);
	ui::v2::DispatchClick(tree, ctx);
	return true;
}

bool Game::DispatchV2ModalKey(int sdl_scancode){
	if(ui_v2_modal_stack.empty()) return false;
	V2ModalEntry & top = ui_v2_modal_stack.back();
	switch(sdl_scancode){
		case SDL_SCANCODE_RETURN:{
			if(top.kind == V2ModalEntry::MESSAGE){
				if(top.has_ok){
					auto cb = std::move(top.on_close);
					PopV2Modal();
					if(cb) cb();
				}
			}else{
				std::string captured = top.password_buf;
				auto cb = std::move(top.on_submit);
				PopV2Modal();
				if(cb) cb(captured);
			}
			return true;
		}
		case SDL_SCANCODE_ESCAPE:{
			// MESSAGE with OK + PASSWORD both dismiss on ESC. Progress (has_ok=false)
			// has no user-driven dismiss path — caller pops when its work finishes.
			if(top.kind == V2ModalEntry::MESSAGE && !top.has_ok) return true;
			if(top.kind == V2ModalEntry::MESSAGE){
				auto cb = std::move(top.on_close);
				PopV2Modal();
				if(cb) cb();
			}else{
				PopV2Modal();
			}
			return true;
		}
		case SDL_SCANCODE_BACKSPACE:{
			if(top.kind == V2ModalEntry::PASSWORD && !top.password_buf.empty()){
				top.password_buf.pop_back();
			}
			return true;
		}
		default: break;
	}
	// Always swallow keys while a modal is up — the underlying state
	// shouldn't receive them.
	return true;
}

bool Game::DispatchV2ModalText(char ascii){
	if(ui_v2_modal_stack.empty()) return false;
	V2ModalEntry & top = ui_v2_modal_stack.back();
	if(top.kind == V2ModalEntry::PASSWORD){
		// Mirror legacy TextInput maxchars=20 cap (password_modal.cpp:47).
		if(top.password_buf.size() < 20) top.password_buf.push_back(ascii);
	}
	return true;
}

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