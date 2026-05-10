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
#include "main_menu_screen.h"
#include "options_screen.h"
#include "options_controls_screen.h"
#include "options_display_screen.h"
#include "options_audio_screen.h"
#include "lobby_connect_screen.h"
#include "lobby_screen.h"
#include "update_screen.h"
#include "mission_summary_screen.h"
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
	lobbyinterface = 0;
	gamecreateinterface = 0;
	gamejoininterface = 0;
	gametechinterface = 0;
	gameselectinterface = 0;
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
		renderer.Draw(&screenbuffer, 1 - (float(tickcheck - lasttick) / wait));
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
				PushScreen(std::make_unique<MainMenuScreen>());
				world.GetAuthorityPeer()->controlledlist.push_back(currentinterface);
				renderer.camera.SetPosition(320, 240);
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				// Button-click handling lives in MainMenuScreen::Tick, dispatched
				// by TickActiveScreen() at the top of Game::Tick.
			}
		}break;
		case LOBBYCONNECT:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				world.lobby.ClearGames();
				world.lobby.state = Lobby::WAITING;
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				PushScreen(std::make_unique<LobbyConnectScreen>());
				world.GetAuthorityPeer()->controlledlist.push_back(currentinterface);
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
				renderer.camera.SetPosition(320, 240);
				lobbyinterface = 0;
				chatinterface = 0;
				gameselectinterface = 0;
				gamecreateinterface = 0;
				gamejoininterface = 0;
				gametechinterface = 0;
				world.choosingtech = false;
				world.lobby.channelchanged = true;
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				PushScreen(std::make_unique<LobbyScreen>());
				world.GetAuthorityPeer()->controlledlist.push_back(currentinterface);
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
				renderer.palette.SetPalette(2);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				PushScreen(std::make_unique<UpdateScreen>());
				world.GetAuthorityPeer()->controlledlist.push_back(currentinterface);
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
			}
		}break;
		case INGAME:{
			if(/*!world.map.loaded && */stateisnew){
				for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
					Object * object = *it;
					switch(object->type){
						case ObjectTypes::TEAM:{
							Team * team = static_cast<Team *>(object);
							team->DestroyOverlays(world);
						}break;
						case ObjectTypes::INTERFACE:{
							Interface * iface = static_cast<Interface *>(object);
							iface->DestroyInterface(world, iface);
						}break;
					}
				}
				screenbuffer.Clear(0);
				//char mapname[7 + 256];
				//sprintf(mapname, "level/%s", world.gameinfo.mapname);
				if(!LoadMap(mapDownloader.FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).c_str())){
					printf("Unable to load map\n");
					if(world.replay.IsPlaying()){
						world.replay.EndPlaying();
					}
					world.Disconnect();
					if(world.lobby.state == Lobby::AUTHENTICATED){
						world.lobby.JoinChannel(world.lobby.lastchannel);
						GoToState(LOBBY);
					}else{
						GoToState(MAINMENU);
					}
					break;
				}
				State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
				if(sharedstateobject){
					sharedstateobject->state = 2;
				}
				if(world.replay.IsRecording()){
					world.replay.WriteStart();
				}
				ShowDeployMessage();
				Audio::GetInstance().StopMusic();
				world.gameplaystate = World::INGAME;
				for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
					Object * object = *it;
					switch(object->type){
						case ObjectTypes::TEAM:{
							Team * team = static_cast<Team *>(object);
							for(int i = 0; i < team->numpeers; i++){
								Peer * peer = world.peerlist[team->peers[i]];
								if(peer){
									world.ingameusers.push_back(peer->accountid);
									User * user = world.lobby.GetUserInfo(peer->accountid);
									if(user){
										user->statsagency = team->agency;
										user->teamnumber = team->number;
									}
									Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
									if(player){
										world.map.RandomPlayerStartLocation(world, player->x, player->y);
										player->oldx = player->x;
										player->oldy = player->y;
										Uint8 teamcolor = team->GetColor();
										player->suitcolor = teamcolor;//(((teamcolor >> 4) - i) << 4) + (teamcolor & 0xF);
										peer->controlledlist.clear();
										peer->controlledlist.push_back(player->id);
										GiveDefaultItems(*player);
									}
								}
							}
						}break;
					}
				}
				world.SendPeerList();
				currentinterface = 0;
				renderer.palette.SetPalette(0);
				renderer.palette.SetParallaxColors(world.map.parallax);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				ambienceMixer.LoadRandomGameMusic();
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					//Audio::GetInstance().ambienceMixer.PlayMusic(world.resources.gamemusic);
					ambienceMixer.PlayMusic(world.resources.gamemusic);
				}
				if(world.replay.IsPlaying()){
					// replay controls
					if(world.localpeerid == world.authoritypeer && !deploymessageshown){
						for(int i = 0; i < world.maxpeers; i++){
							if(world.peerlist[i] && i != world.authoritypeer){
								world.localpeerid = i;
								break;
							}
						}
					}
					world.replay.oldx = world.replay.x;
					world.replay.oldy = world.replay.y;
					if(world.localinput.keymoveleft || world.localinput.keymoveright || world.localinput.keymoveup || world.localinput.keymovedown){
						world.localpeerid = world.authoritypeer;
						bool inbase = false;
						if(world.replay.y > world.map.height * 64){
							inbase = true;
						}
						if(world.localinput.keymoveleft){
							if(world.replay.xv > 0){
								world.replay.xv = 0;
							}
							world.replay.xv -= 3;
							world.replay.x += world.replay.xv;
							if(world.replay.x < 320){
								world.replay.x = 320;
							}
						}else
						if(world.localinput.keymoveright){
							if(world.replay.xv < 0){
								world.replay.xv = 0;
							}
							world.replay.xv += 3;
							world.replay.x += world.replay.xv;
							if(world.replay.x > ((inbase ? world.map.expandedwidth : world.map.width) * 64) - 320){
								world.replay.x = ((inbase ? world.map.expandedwidth : world.map.width) * 64) - 320;
							}
						}else{
							world.replay.xv = 0;
						}
						if(world.localinput.keymoveup){
							if(world.replay.yv > 0){
								world.replay.yv = 0;
							}
							world.replay.yv -= 3;
							world.replay.y += world.replay.yv;
							if(world.replay.y < 240){
								world.replay.y = 240;
							}
						}else
						if(world.localinput.keymovedown){
							if(world.replay.yv < 0){
								world.replay.yv = 0;
							}
							world.replay.yv += 3;
							world.replay.y += world.replay.yv;
							if(world.replay.y > ((inbase ? world.map.expandedheight : world.map.height) * 64) - 240){
								world.replay.y = ((inbase ? world.map.expandedheight : world.map.height) * 64) - 240;
							}
						}else{
							world.replay.yv = 0;
						}
					}else{
						world.replay.xv = 0;
						world.replay.yv = 0;
					}
					if(world.localinput.keyprevcam && !world.localinputhistory[(world.tickcount - 1) % world.maxlocalinputhistory].keyprevcam){
						for(int i = world.localpeerid - 1; i > 0; i--){
							if(world.peerlist[i] && i != world.authoritypeer){
								world.localpeerid = i;
								break;
							}
						}
					}
					if(world.localinput.keynextcam && !world.localinputhistory[(world.tickcount - 1) % world.maxlocalinputhistory].keynextcam){
						for(int i = world.localpeerid + 1; i < world.maxpeers; i++){
							if(world.peerlist[i] && i != world.authoritypeer){
								world.localpeerid = i;
								break;
							}
						}

					}
					world.replay.speed = 1;
					if(world.localinput.keydetonate){
						world.replay.speed = 0.05;
					}
					if(world.localinput.keyuse){
						world.replay.speed = 2;
					}
					if(world.localinput.keyjump){
						world.replay.showallnames = true;
					}else{
						world.replay.showallnames = false;
					}
					//
					if(!world.replay.ReadToNextTick(world)){
						world.replay.EndPlaying();
						GoToState(MAINMENU);
					}
				}
				Peer * localpeer = world.peerlist[world.localpeerid];
				if(localpeer && world.localpeerid != world.authoritypeer){
					if(localpeer->controlledlist.size() == 0){
						world.RequestPeerList();
					}
				}
				if(!deploymessageshown && world.messagetype == 1 && world.message_i == 63){
					world.ShowMessage((char *)"Location : Base Arsia Mons, Surface Temperature : -7C", 96, 1);
					deploymessageshown = true;
				}
				if(CheckForQuit()){
					world.Disconnect();
					if(world.lobby.state == Lobby::AUTHENTICATED){
						GoToState(LOBBY);
						world.lobby.JoinChannel(world.lobby.lastchannel);
					}else{
						if(world.replay.IsPlaying()){
							world.replay.EndPlaying();
						}
						GoToState(MAINMENU);
					}
				}
				if(CheckForEndOfGame()){
					if(world.lobby.state == Lobby::AUTHENTICATED){
						GoToState(MISSIONSUMMARY);
					}else{
						if(world.replay.IsPlaying()){
							world.replay.EndPlaying();
						}
						GoToState(MAINMENU);
					}
				}
				if(CheckForConnectionLost()){
					if(world.lobby.state == Lobby::AUTHENTICATED){
						GoToState(LOBBY);
						world.lobby.JoinChannel(world.lobby.lastchannel);
					}else{
						GoToState(MAINMENU);
					}
				}
			}
		}break;
		case MISSIONSUMMARY:{
			if(stateisnew){
				UnloadGame();
				world.Disconnect();
				renderer.camera.SetPosition(320, 240);
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				PushScreen(std::make_unique<MissionSummaryScreen>());
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
			}
		}break;
		case SINGLEPLAYERGAME:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				world.gameplaystate = World::INGAME;
				world.intutorialmode = true;
				currentinterface = 0;
				world.GetAuthorityPeer()->techchoices = World::BUY_LASER | World::BUY_ROCKET;
				//world.Listen(23456);
				Team * team = (Team *)world.CreateObject(ObjectTypes::TEAM);
				team->AddPeer(world.GetAuthorityPeer()->id);
				team->agency = Team::NOXIS;
				team->color = ((8 << 4) + 13);
				Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
				player->suitcolor = team->color;
				player->laserammo = 0;
				player->credits = GASLoader::Get().player.startingCredits;
				player->RemoveInventoryItem(Player::INV_BASEDOOR);
				ShowDeployMessage();
				world.GetAuthorityPeer()->controlledlist.push_back(player->id);
				world.gameinfo.securitylevel = LobbyGame::SECNONE;
				if(!LoadMap((GetResDir() + "AGENCY04.SIL").c_str())){
					GoToState(MAINMENU);
				}
				Audio::GetInstance().StopMusic();
				world.map.RandomPlayerStartLocation(world, player->x, player->y);
				player->oldx = player->x;
				player->oldy = player->y;
				renderer.palette.SetPalette(0);
				renderer.palette.SetParallaxColors(world.map.parallax);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				singleplayermessage = 0;
				stateisnew = false;
				ambienceMixer.LoadRandomGameMusic();
			}
			if(ambienceMixer.FadedIn()){
				//Audio::GetInstance().ambienceMixer.PlayMusic(world.resources.gamemusic);
				ambienceMixer.PlayMusic(world.resources.gamemusic);
			}
			Player * player = world.GetPeerPlayer(world.localpeerid);
			if(player){
				switch(singleplayermessage){
					case 0:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Move your agent left and right\nBy tapping %s and %s.", GetActionKeyDisplayName(Action::MoveLeft), GetActionKeyDisplayName(Action::MoveRight));
							world.ShowMessage(text, 128);
						}
						if(player->state == Player::RUNNING){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 1:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Make your agent jump by striking %s.", GetActionKeyDisplayName(Action::Jump));
							world.ShowMessage(text, 128);
						}
						if(player->state == Player::JUMPING){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 2:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "If you hold the %s key down, it will\nactivate your agent's jet-pack.", GetActionKeyDisplayName(Action::Jetpack));
							world.ShowMessage(text, 128);
						}
						if(player->state == Player::JETPACK){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 3:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Make your agent kneel by holding %s.", GetActionKeyDisplayName(Action::MoveDown));
							world.ShowMessage(text, 128);
						}
						if(player->state == Player::CROUCHED){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 4:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Make your agent roll by kneeling,\nthen striking %s or %s.", GetActionKeyDisplayName(Action::MoveLeft), GetActionKeyDisplayName(Action::MoveRight));
							world.ShowMessage(text, 128);
						}
						if(player->state == Player::ROLLING){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 5:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "To disguise as a civilian, press the %s key.", GetActionKeyDisplayName(Action::Disguise));
							world.ShowMessage(text, 128);
						}
						if(player->IsDisguised()){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 6:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "To return to normal, press the %s key again.", GetActionKeyDisplayName(Action::Disguise));
							world.ShowMessage(text, 128);
						}
						if(!player->IsDisguised()){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 7:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "The %s key fires your current weapon,\nthe Blaster.", GetActionKeyDisplayName(Action::Fire));
							world.ShowMessage(text, 128);
						}
						if(player->state == Player::STANDINGSHOOT || player->state == Player::CROUCHEDSHOOT || player->state == Player::FALLINGSHOOT || player->state == Player::JETPACKSHOOT || player->state == Player::LADDERSHOOT){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 8:{
						if(!world.message_i){
							char text[256];
#ifdef OUYA
							sprintf(text, "To change weapons, press %s", GetActionKeyDisplayName(Action::NextWeapon));
#else
							sprintf(text, "To change weapons, press the 1, 2, 3, or 4 keys");
#endif
							world.ShowMessage(text, 128);
							
						}
						if(player->laserammo == 0){
							player->laserammo = 15;
						}
						if(player->currentweapon != 0){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 9:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Good job, agent.\n\nYou have been given a base-building device.");
							world.ShowMessage(text, 128);
							player->AddInventoryItem(Player::INV_BASEDOOR);
							singleplayermessage++;
						}
					}break;
					case 10:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Hit %s to build your base.", GetActionKeyDisplayName(Action::Use));
							world.ShowMessage(text, 128);
						}
						Team * team = player->GetTeam(world);
						if(team && team->basedoorid){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 11:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "To enter your base, hit %s when your\nSilencer is at the base entrance.", GetActionKeyDisplayName(Action::Activate));
							world.ShowMessage(text, 128);
						}
						if(player->InBase(world)){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 12:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "You are now inside your agent's secret base.\nWalk right to the flashing green computer screen\nand hit %s to activate it.", GetActionKeyDisplayName(Action::Activate));
							world.ShowMessage(text, 255);
						}
						if(!player->InBase(world)){
							singleplayermessage = 11;
							world.message_i = 0;
						}
						if(player->buyinterfaceid){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 13:{
						if(!world.message_i){
							char text[256];
#ifdef OUYA
							const char * button = "O";
#else
							const char * button = "Enter";
#endif
							sprintf(text, "Use the Up and Down keys to select Rocket ammo\nand press %s to purchase.", button);
							world.ShowMessage(text, 255);
						}
						if(!player->InBase(world)){
							singleplayermessage = 11;
							world.message_i = 0;
						}
						if(player->credits < 250){
						player->credits = GASLoader::Get().player.creditFloor;
						}
						if(player->rocketammo > 0){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 14:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Good, now hit %s or %s to exit the menu.", GetActionKeyDisplayName(Action::MoveLeft), GetActionKeyDisplayName(Action::MoveRight));
							world.ShowMessage(text, 128);
						}
						if(player->rocketammo > 0){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 15:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "To leave your base, walk Left through\nthe door you entered.");
							world.ShowMessage(text, 128);
						}
						if(!player->InBase(world)){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 16:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "In the playfield, you need to hack into\ndata terminals to collect information.");
							world.ShowMessage(text, 192);
						}
						if(world.message_i >= 192 - 1){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 17:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Walk around until you see a\nflashing green data port.\nStanding in front of the data port, hit %s\nto initiate hacking.", GetActionKeyDisplayName(Action::Activate));
							world.ShowMessage(text, 255);
						}
						if(player->state == Player::HACKING && player->files >= 100){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 18:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Return with the information to your base door,\nhitting %s to enter the base.", GetActionKeyDisplayName(Action::Activate));
							world.ShowMessage(text, 128);
						}
						if(player->InBase(world)){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 19:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Walk to the Right, through the agency receiver\nto deliver the information to your agency.");
							world.ShowMessage(text, 128);
						}
						if(!player->InBase(world)){
							singleplayermessage = 18;
							world.message_i = 0;
						}
						if(!player->files){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 20:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Good job, agent.\nYou're ready for the final training exercise.");
							world.ShowMessage(text, 128);
						}
						if(world.message_i >= 128 - 1){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 21:{
						if(!world.message_i){
							world.highlightsecrets = true;
							char text[256];
							sprintf(text, "This indicator shows your progress towards\ndiscovering the location of a secret.\nKeep collecting files\nuntil all stages are lit.");
							world.ShowMessage(text, 255);
						}
						Team * team = player->GetTeam(world);
						if(team && team->beamingterminalid){
							world.highlightsecrets = false;
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 22:{
						if(!world.message_i){
							world.highlightminimap = true;
							char text[256];
							sprintf(text, "The narrowing circle on your radar shows\nyour agency acquiring a lock on the secret.\nWhen the lock is completed, the\nsecret can be picked up by your team.");
							world.ShowMessage(text, 255);
						}
						Team * team = player->GetTeam(world);
						bool advance22 = player->hassecret || (team && team->secrets > 0);
						if(!advance22 && team && team->beamingterminalid){
							Terminal * terminal = static_cast<Terminal *>(world.GetObjectFromId(team->beamingterminalid));
							if(terminal){
								if(terminal->beamingtime > 45){
									terminal->beamingtime = 45;
								}
								if(terminal->state == Terminal::SECRETREADY){
									advance22 = true;
								}
							}
						}
						if(advance22){
							world.highlightminimap = false;
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 23:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Pick up the secret at the location shown\non your radar map");
							world.ShowMessage(text, 128);
						}
						Team * team23 = player->GetTeam(world);
						if(player->hassecret || (team23 && team23->secrets > 0)){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 24:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Now, you must return the secret to your base.\nIf this were a real government secret,\nyou would have limited time before\nthe government traced your location.");
							world.ShowMessage(text, 255);
						}
						Team * team24 = player->GetTeam(world);
						if(player->InBase(world) || (team24 && team24->secrets > 0)){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 25:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "To stash the secret data, it must be brought\nto the memory bank at the\nfar right of your base.");
							world.ShowMessage(text, 128);
						}
						Team * team = player->GetTeam(world);
						if(team && team->secrets > 0){
							singleplayermessage++;
							world.message_i = 0;
						}
					}break;
					case 26:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Good job, agent.\n\nYou're ready to begin real agency missions.");
							world.ShowMessage(text, 255);
						}
						if(world.message_i == 12){
							player->state = Player::UNDEPLOYING;
							player->state_i = 0;
						}
						if(world.message_i >= 128 - 1){
							GoToState(MAINMENU);
						}
					}break;
				}
			}
			if(CheckForQuit() || CheckForEndOfGame()){
				GoToState(MAINMENU);
			}
		}break;
		case OPTIONS:{
			if(stateisnew){
				world.DestroyAllObjects();
				PushScreen(std::make_unique<OptionsScreen>());
				stateisnew = false;
			}
		}break;
		case OPTIONSCONTROLS:{
			if(stateisnew){
				world.DestroyAllObjects();
				PushScreen(std::make_unique<OptionsControlsScreen>());
				stateisnew = false;
			}
		}break;
		case OPTIONSDISPLAY:{
			if(stateisnew){
				world.DestroyAllObjects();
				PushScreen(std::make_unique<OptionsDisplayScreen>());
				stateisnew = false;
			}
		}break;
		case OPTIONSAUDIO:{
			if(stateisnew){
				world.DestroyAllObjects();
				PushScreen(std::make_unique<OptionsAudioScreen>());
				stateisnew = false;
			}
		}break;
		case HOSTGAME:{
			if(stateisnew){
				world.lagsimulator.Activate(200, 200, 0.0f);
				world.Listen(12456);
				world.DestroyAllObjects();
				Audio::GetInstance().StopMusic();
				world.gameplaystate = World::INLOBBY;
				world.gameinfo.accountid = 1;
				world.dedicatedserver.active = true;
				world.dedicatedserver.accountid = 1;
				currentinterface = 0;
				State * newsharedstateobject = static_cast<State *>(world.CreateObject(ObjectTypes::STATE));
				sharedstate = newsharedstateobject->id;
				newsharedstateobject->state = 0;
				world.replay.BeginRecording("testrecording.zsr");
				if(world.replay.IsRecording()){
					world.replay.WriteHeader(world);
					world.replay.WriteGameInfo(world.gameinfo);
				}
				stateisnew = false;
			}
			mapDownloader.ProcessMapDownload();
			/*if(world.tickcount % 48 == 0){
				world.SendPeerList();
			}*/
			if(!world.map.loaded && world.peercount >= 1 && world.AllPeersLoadedGameInfo() && world.AllPeersDownloadedMap()){
				screenbuffer.Clear(0);
				if(world.replay.IsRecording()){
					world.replay.WriteStart();
				}
				//char mapname[256];
				//sprintf(mapname, "level/%s", world.gameinfo.mapname);
				LoadMap(mapDownloader.FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).c_str());
				renderer.palette.SetPalette(0);
				renderer.palette.SetParallaxColors(world.map.parallax);
				SetColors(renderer.palette.GetColors());
				State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
				if(sharedstateobject){
					sharedstateobject->state = 2;
				}
				//world.GetAuthorityPeer()->controlledlist.clear();
				world.gameplaystate = World::INGAME;
				for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
					Object * object = *it;
					switch(object->type){
						case ObjectTypes::TEAM:{
							Team * team = static_cast<Team *>(object);
							if(team){
								for(int i = 0; i < team->numpeers; i++){
									Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
									if(player){
										world.map.RandomPlayerStartLocation(world, player->x, player->y);
										player->oldx = player->x;
										player->oldy = player->y;
										//player->AddInventoryItem(Player::INV_VIRUS);
										player->credits = GASLoader::Get().player.startingCredits;
										Uint8 teamcolor = team->GetColor();
										player->suitcolor = (((teamcolor >> 4) - i) << 4) + (teamcolor & 0xF);
										for(int j = 0; j < 5; j++){
											world.peerlist[team->peers[i]]->techchoices |= 1 << (rand() % 10);
										}
										world.peerlist[team->peers[i]]->techchoices = 0xffffffff;
										world.peerlist[team->peers[i]]->controlledlist.clear();
										world.peerlist[team->peers[i]]->controlledlist.push_back(player->id);
										GiveDefaultItems(*player);
									}
								}
							}
						}break;
					}
				}
				world.SendPeerList();
			}
		}break;
		case JOINGAME:{
			if(stateisnew){
				strcpy(world.gameinfo.mapname, "STAR72.SIL");
				mapDownloader.CalculateMapHash(mapDownloader.FindMap(world.gameinfo.mapname).c_str(), &world.gameinfo.maphash);
				world.gameinfo.accountid = 1;
				world.gameinfo.loaded = true;
				sharedstate = 0;
				Peer * authoritypeer = world.GetAuthorityPeer();
				authoritypeer->ip = ntohl(inet_addr("127.0.0.1"));
				authoritypeer->port = 12456;
				world.Connect(rand() % 5, 1);
				mapDownloader.LoadMapData(mapDownloader.FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).c_str());
				//printf("map data: %d %d\n", world.currentmapdatalength, world.currentmapdatamax);
				Audio::GetInstance().StopMusic();
				currentinterface = 0;
				world.DestroyAllObjects();
				stateisnew = false;
			}else{
				State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
				if(sharedstateobject && sharedstateobject->state == 2){
					world.gameplaystate = World::INGAME;
				}
				mapDownloader.ProcessMapDownload();
			}
		}break;
		case TESTGAME:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				world.gameplaystate = World::INGAME;
				currentinterface = 0;
				Audio::GetInstance().StopMusic();
				world.GetAuthorityPeer()->techchoices = 0xffffffff;//World::BUY_LASER | World::BUY_ROCKET;
				Team * team = (Team *)world.CreateObject(ObjectTypes::TEAM);
				team->AddPeer(world.GetAuthorityPeer()->id);
				team->agency = Team::LAZARUS;
				//team->color = ((8 << 4) + 13);
				Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
				player->suitcolor = team->GetColor();
				player->laserammo = 0;
				player->credits = GASLoader::Get().player.creditCap;
				player->oldx = player->x;
				player->oldy = player->y;
				world.GetAuthorityPeer()->controlledlist.push_back(player->id);
				GiveDefaultItems(*player);
				int botnum = 0;
				// Spawn a mix of difficulties: 4 easy, 4 medium, 2 hard
				const PlayerAI::Difficulty diffs[10] = {
					PlayerAI::EASY, PlayerAI::EASY, PlayerAI::EASY, PlayerAI::EASY,
					PlayerAI::MEDIUM, PlayerAI::MEDIUM, PlayerAI::MEDIUM, PlayerAI::MEDIUM,
					PlayerAI::HARD, PlayerAI::HARD
				};
				for(int i = 0; i < 10; i++){
					Uint8 agency;
					do{
						agency = rand() % 5;
					}while(agency == Team::BLACKROSE);
					Peer * botpeer = world.AddBot(agency);
					if(botpeer){
						botpeer->accountid = 0xFFFFFFFF - botnum;
						Team * botteam = world.GetPeerTeam(botpeer->id);
						Player * botplayer = (Player *)world.CreateObject(ObjectTypes::PLAYER);
						botplayer->suitcolor = botteam->GetColor();
						botplayer->laserammo = 0;
						botplayer->credits = GASLoader::Get().player.startingCredits;
						botplayer->ai = new PlayerAI(*botplayer, diffs[botnum]);
						botpeer->controlledlist.push_back(botplayer->id);
						world.map.RandomPlayerStartLocation(world, botplayer->x, botplayer->y);
						botplayer->oldx = botplayer->x;
						botplayer->oldy = botplayer->y;
						botnum++;
					}
				}
				world.gameinfo.securitylevel = LobbyGame::SECHIGH;
				LoadMap("level/ALLY10c.sil");
				for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
					if((*it)->type == ObjectTypes::PLAYER){
						Player * player = static_cast<Player *>(*it);
						world.map.RandomPlayerStartLocation(world, player->x, player->y);
					}
				}
				ShowDeployMessage();
				renderer.palette.SetPalette(0);
				renderer.palette.SetParallaxColors(world.map.parallax);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				singleplayermessage = 0;
				stateisnew = false;
			}else{
				/*Player * localplayer = world.GetPeerPlayer(world.authoritypeer);
				if(localplayer){
					if(localplayer->state == Player::STANDINGSHOOT){
						for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
							if((*it)->type == ObjectTypes::PLAYER){
								Player * player = static_cast<Player *>(*it);
								if(player->ai){
									//player->ai->SetTarget(world, localplayer->x, localplayer->y);
									player->hassecret = true;
									break;
								}
							}
						}
					}
				}*/
				if(CheckForQuit() || CheckForEndOfGame()){
					GoToState(MAINMENU);
				}
			}
		}break;
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
	if(gamejoininterface || gametechinterface){
		world.Disconnect();
		LobbyScreen * lobby = screenStack.empty() ? nullptr : dynamic_cast<LobbyScreen *>(screenStack.back().get());
		if(lobby) lobby->SetMapNameOverlay(world, "");
		world.lobby.gamesprocessed = false;
		world.lobby.channelchanged = true;
		world.SwitchToLocalAuthorityMode();
		sharedstate = 0;
		// Destroy the team overlays now that we're leaving the joined-game
		// surface. ShowGameSelect (via TearDownRightPanels) owns the panel
		// iface teardown + choosingtech / ShowTeamOverlays(true) reset.
		for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
			Object * object = *it;
			if(object->type == ObjectTypes::TEAM){
				Team * team = static_cast<Team *>(object);
				team->DestroyOverlays(world);
				world.MarkDestroyObject(object->id);
			}
		}
		world.lobby.JoinChannel(world.lobby.lastchannel);
		if(lobby) lobby->ShowGameSelect(screenContext);
		currentinterface = lobbyinterface;
		return true;
	}else
	if(gamecreateinterface){
		// LobbyScreen::ShowGameSelect tears down the active gameCreate panel
		// and rebuilds gameSelect; it also resets ctx.game.gamecreateinterface.
		world.lobby.gamesprocessed = false;
		LobbyScreen * lobby = screenStack.empty() ? nullptr : dynamic_cast<LobbyScreen *>(screenStack.back().get());
		if(lobby){
			lobby->ShowGameSelect(screenContext);
		}
		currentinterface = lobbyinterface;
		return true;
	}else{
		GoToState(MAINMENU);
	}
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