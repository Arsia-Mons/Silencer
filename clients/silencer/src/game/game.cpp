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
#include "lobby_connect_screen.h"
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
#include "audio.h"
#include "renderdevice.h"
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_timer.h>
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
		}else{
			renderer.Draw(&screenbuffer, 1 - (float(tickcheck - lasttick) / wait));
		}
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
				PushScreen(std::make_unique<LobbyConnectScreen>());
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
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
				PushScreen(std::make_unique<LobbyScreen>());
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				// Lobby pump (state-machine + deferred-create) lives in
				// LobbyScreen::Tick, dispatched by TickActiveScreen() at the
				// top of Game::Tick.
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
				PushScreen(std::make_unique<MissionSummaryScreen>());
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