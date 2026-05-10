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
#include "main_menu_screen.h"
#include "options_screen.h"
#include "options_controls_screen.h"
#include "options_display_screen.h"
#include "options_audio_screen.h"
#include "lobby_connect_screen.h"
#include "lobby_screen.h"
#include "update_screen.h"
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
               screenContext(*this, world, renderer, world.lobby, keymap, updater, ambienceMixer, window, renderdevice){
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
	mappreviewinterface = 0;
	modalinterface = 0;
	sharedstate = 0;
	currentlobbygameid = 0;
	lastannouncedgameid = 0;
	lastannouncedstatus = 0;
	joininggame = false;
	memset(keystate, 0, sizeof(keystate));
	gamepad = nullptr;
	singleplayermessage = 0;
	updatetitle = true;
	oldselectedagency = -1;
	agencychanged = true;
	currentinterface = 0;
	lastchannel[0] = 0;
	minimized = false;
	modaldialoghasok = false;
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
		}while((cmdline = strtok(0, " ")));
	}
	Config::GetInstance().Load();
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
		}
		world.SendInput();
		if(!Tick()){
			return false;
		}
		if(!world.replay.IsPlaying() || (world.replay.IsPlaying() && world.gameplaystate == World::INGAME)){
			world.Tick();
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
			if(gamejoininterface){
				Interface * gamejoiniface = static_cast<Interface *>(world.GetObjectFromId(gamejoininterface));
				if(gamejoiniface){
					Button * readybtn = static_cast<Button *>(gamejoiniface->GetObjectWithUid(world, 25));
					if(readybtn){
						Peer * localpeer = world.peerlist[world.localpeerid];
						bool blocked = localpeer && localpeer->ishost && !world.AllPeersDownloadedMap();
						strcpy(readybtn->text, blocked ? "Waiting..." : "Ready");
					}
				}
			}
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
		case FADEOUT:{
			world.intutorialmode = false;
			SDL_Color * fadedpalette = renderer.palette.CopyWithBrightness(renderer.palette.GetColors(), (15 - fade_i) * 8);
			SetColors(fadedpalette);
			if(fade_i >= 16){
				state = nextstate;
				fade_i = 0;
				stateisnew = true;
			}
		}break;
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
				agencychanged = true;
				UnloadGame();
				world.Disconnect();
				renderer.camera.SetPosition(320, 240);
				lobbyinterface = 0;
				characterinterface = 0;
				chatinterface = 0;
				gameselectinterface = 0;
				gamecreateinterface = 0;
				gamejoininterface = 0;
				gametechinterface = 0;
				gamesummaryinterface = 0;
				modalinterface = 0;
				passwordinterface = 0;
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
						world.lobby.JoinChannel(lastchannel);
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
						world.lobby.JoinChannel(lastchannel);
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
						world.lobby.JoinChannel(lastchannel);
					}else{
						GoToState(MAINMENU);
					}
				}
			}
		}break;
		case MISSIONSUMMARY:{
			if(stateisnew){
				UnloadGame();
				gamesummaryinfoloaded = false;
				world.Disconnect();
				renderer.camera.SetPosition(320, 240);
				User * user = world.lobby.GetUserInfo(world.lobby.accountid);
				gamesummaryinterface = CreateGameSummaryInterface(user->statscopy, user->statsagency)->id;
				currentinterface = gamesummaryinterface;
				renderer.palette.SetPalette(1);
				screenbuffer.Clear(0);
				SetColors(renderer.palette.GetColors());
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					//Audio::GetInstance().ambienceMixer.PlayMusic(world.resources.menumusic);
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				Interface * gamesummaryiface = static_cast<Interface *>(world.GetObjectFromId(gamesummaryinterface));
				if(gamesummaryiface){
					ProcessGameSummaryInterface(gamesummaryiface);
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
						if(team && team->beamingterminalid){
							Terminal * terminal = static_cast<Terminal *>(world.GetObjectFromId(team->beamingterminalid));
							if(terminal){
								if(terminal->beamingtime > 45){
									terminal->beamingtime = 45;
								}
								if(terminal->state == Terminal::SECRETREADY){
									world.highlightminimap = false;
									singleplayermessage++;
									world.message_i = 0;
								}
							}
						}
					}break;
					case 23:{
						if(!world.message_i){
							char text[256];
							sprintf(text, "Pick up the secret at the location shown\non your radar map");
							world.ShowMessage(text, 128);
						}
						if(player->hassecret){
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
						if(player->InBase(world)){
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
		case REPLAYGAME:{
			if(stateisnew){
				world.Disconnect();
				world.lobby.Disconnect();
				UnloadGame();
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				stateisnew = false;
				world.gameplaystate = World::INLOBBY;
				world.replay.BeginPlaying(replayfile);
				if((world.replay.IsPlaying() && !world.replay.ReadHeader(world)) || !world.replay.IsPlaying()){
					printf("Replay error\n");
					world.replay.EndPlaying();
					GoToState(MAINMENU);
				}
			}else{
				while(world.replay.ReadToNextTick(world)){
					if(world.replay.GameStarted()){
						GoToState(INGAME);
						break;
					}
					world.Tick();
				}
				if(!world.replay.GameStarted()){
					world.replay.EndPlaying();
					GoToState(MAINMENU);
				}
			}
		}break;
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

bool Game::LoadMap(const char * name){
	if(!world.map.Load(name, world)){
		return false;
	}
	if(!world.dedicatedserver.active){
		ambienceMixer.CreateAmbienceChannels();
		renderer.palette.SetParallaxColors(world.map.parallax);
	}
	return true;
}

void Game::UnloadGame(void){
	Audio::GetInstance().StopAll(GASLoader::Get().gameengine.audioStopAllFadeMs);
	currentlobbygameid = 0;
	for(int i = 0; i < sizeof(ambienceMixer.bgchannel) / sizeof(int); i++){
		ambienceMixer.bgchannel[i] = -1;
	}
	world.SwitchToLocalAuthorityMode();
	if(world.map.loaded){ // if the map was loaded, then the music was played
		Audio::GetInstance().StopMusic();
	}
	world.map.Unload();
	world.pancameraactive = false;
	world.pancamerareturn = false;
	world.pancamerareturncount = 0;
	world.message_i = 0;
	world.winningteamid = 0;
	world.DestroyAllObjects();
	world.chatlines.clear();
	world.messagetype = 0;
	world.highlightminimap = false;
	world.highlightsecrets = false;
	world.quitstate = 0;
	world.ingameusers.clear();
}

bool Game::CheckForQuit(void){
	if(keystate[SDL_SCANCODE_RETURN]){
		if(world.quitstate == 1 || world.quitstate == 2){
			world.quitstate = 0;
			return true;
		}
	}
	return false;
}

bool Game::CheckForEndOfGame(void){
	if(world.winningteamid){
		if(world.message_i == GASLoader::Get().gameengine.ticksPerSecond * 3){
			if(world.IsAuthority()){
				if(world.replay.IsRecording()){
					world.replay.EndRecording();
				}
				for(std::vector<Uint32>::iterator it = world.ingameusers.begin(); it != world.ingameusers.end(); it++){
					Uint32 accountid = *it;
					User * user = world.lobby.GetUserInfo(accountid);
					if(user){
						Uint8 won = 0;
						for(int i = 0; i < world.maxpeers; i++){
							Peer * peer = world.peerlist[i];
							if(peer){
								if(peer->accountid == user->accountid){
									Team * team = world.GetPeerTeam(peer->id);
									user->statscopy = peer->stats;
									user->statsagency = team->agency;
									user->teamnumber = team->number;
									world.SendStats(*peer);
									if(team && team->id == world.winningteamid){
										won = 1;
									}
								}
							}
						}
						world.lobby.RegisterStats(*user, won, world.gameinfo.id);
					}
				}
			}
		}
		if(world.message_i >= 240){
			return true;
		}
	}
	return false;
}

bool Game::CheckForConnectionLost(void){
	if(world.replay.IsPlaying()){
		return false;
	}
	if(world.state == World::IDLE && world.message_i >= 48){
		return true;
	}
	return false;
}

void Game::ProcessInGameInterfaces(void){
	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	if(localplayer){
		if(localplayer->buyinterfaceid || localplayer->techinterfaceid){
			currentinterface = localplayer->buyinterfaceid;
			if(!currentinterface){
				currentinterface = localplayer->techinterfaceid;
			}
		}else{
			oldselecteditem = 0;
		}
		if(localplayer->chatinterfaceid){
			currentinterface = localplayer->chatinterfaceid;
		}
		if(!localplayer->chatinterfaceid && !localplayer->buyinterfaceid && !localplayer->techinterfaceid){
			currentinterface = 0;
		}
		Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
		if(iface){
			if(iface->id == localplayer->chatinterfaceid){
				TextInput * textinput = (TextInput *)iface->GetObjectWithUid(world, 1);
				if(textinput){
					if(textinput->tabpressed){
						localplayer->chatwithteam = !localplayer->chatwithteam;
						textinput->tabpressed = false;
					}
					if(textinput->enterpressed){
						if(strlen(textinput->text) > 0){
							world.SendChat(localplayer->chatwithteam, textinput->text);
						}
						iface->DestroyInterface(world, iface);
						localplayer->chatinterfaceid = 0;
					}
					if(keystate[quitscancode]){
						iface->DestroyInterface(world, iface);
						localplayer->chatinterfaceid = 0;
					}
				}
			}else
			if(iface->id == localplayer->buyinterfaceid || iface->id == localplayer->techinterfaceid){
				bool buying = false;
				if(iface->id == localplayer->buyinterfaceid){
					buying = true;
				}
				SelectBox * selectbox = (SelectBox *)iface->GetObjectWithUid(world, 1);
				if(selectbox){
					if(selectbox->selecteditem != oldselecteditem){
						Audio::GetInstance().Play(world.resources.soundbank[GASLoader::Get().player.soundRoundCountdown], 64);
						oldselecteditem = selectbox->selecteditem;
						if(buying){
							localplayer->buyifacelastitem = selectbox->selecteditem;
						}
					}
					if(selectbox->selecteditem >= selectbox->scrolled + 5){
						selectbox->scrolled = selectbox->selecteditem - 4;
					}
					if(selectbox->selecteditem < selectbox->scrolled){
						selectbox->scrolled = selectbox->selecteditem;
					}
					if(buying){
						localplayer->buyifacelastscrolled = selectbox->scrolled;
					}
					if(selectbox->enterpressed){
						BuyableItem * buyableitem = 0;
						for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
							if((*it)->id == selectbox->IndexToId(selectbox->selecteditem)){
								buyableitem = (*it);
								break;
							}
						}
						if(buyableitem){
							if(buying){
								localplayer->BuyItem(world, buyableitem->id);
							}else{
								if(localplayer->InOwnBase(world)){
									localplayer->RepairItem(world, buyableitem->id);
								}else{
									localplayer->VirusItem(world, buyableitem->id);
								}
							}
						}
						selectbox->enterpressed = false;
					}
				}
			}
		}
	}
}

void Game::ShowDeployMessage(void){
	world.ShowMessage((char *)"STANDBY FOR TEAM DEPLOYMENT", 64, 1);
	deploymessageshown = false;
}

void Game::GiveDefaultItems(Player & player){
	Team * team = player.GetTeam(world);
	if(team){
		for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
			BuyableItem * buyableitem = *it;
			switch(buyableitem->id){
				case World::BUY_LASER:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("laser");
						player.laserammo = def ? def->spawnAmmo : 5;
					}
				}break;
				case World::BUY_ROCKET:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("rocket");
						player.rocketammo = def ? def->spawnAmmo : 3;
					}
				}break;
				case World::BUY_FLAMER:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("flamer");
						player.flamerammo = def ? def->spawnAmmo : 15;
					}
				}break;
				case World::BUY_HEALTH:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("healthpack");
						int count = def ? def->spawnInventoryCount : 1;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_HEALTHPACK);
					}
				}break;
				case World::BUY_VIRUS:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("virus");
						int count = def ? def->spawnInventoryCount : 1;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_VIRUS);
					}
				}break;
				case World::BUY_POISON:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("poison");
						int count = def ? def->spawnInventoryCount : 2;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_POISON);
					}
				}break;
				case World::BUY_TRACT:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("lazarustract");
						int count = def ? def->spawnInventoryCount : 1;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_LAZARUSTRACT);
					}
				}break;
			}
		}
	}
}

void Game::JoinGame(LobbyGame & lobbygame, char * password){
	strcpy(world.mapname, lobbygame.mapname);
	Peer * peer = world.GetAuthorityPeer();
	peer->ip = ntohl(inet_addr(lobbygame.hostname));
	//peer->ip = ntohl(inet_addr("127.0.0.1")); // temporary
	peer->port = lobbygame.port;
	sharedstate = 0;
	world.mode = World::REPLICA;
	world.Connect(GetSelectedAgency(), world.lobby.accountid, password);
	joininggame = true;
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

void Game::TickLobbyBody(void){
	if(world.lobby.state == Lobby::DISCONNECTED){
		world.Disconnect();
		GoToState(LOBBYCONNECT);
		return;
	}
	if(lobbyinterface){
		Interface * iface = static_cast<Interface *>(world.GetObjectFromId(lobbyinterface));
		if(iface){
			ProcessLobbyInterface(iface);
		}
	}
	if(gamecreateinterface){
		// Handle deferred CreateGame after map upload completes.
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
			CreateModalDialog("Could not upload map");
		}
		if(world.lobby.creategamestatus == 1){
			world.lobby.creategamestatus = 0;
			Peer * authoritypeer = world.GetAuthorityPeer();
			LobbyGame * lobbygame = world.lobby.GetGameById(world.lobby.createdgameid);
			if(lobbygame){
				Serializer data;
				lobbygame->Serialize(Serializer::WRITE, data);
				world.gameinfo.Serialize(Serializer::READ, data);
				authoritypeer->ip = ntohl(inet_addr(lobbygame->hostname));
				authoritypeer->port = lobbygame->port;
				sharedstate = 0;
				currentlobbygameid = lobbygame->id;
				world.Connect(GetSelectedAgency(), world.lobby.accountid, lobbygame->password);
				mapDownloader.LoadMapData(mapDownloader.FindMap(lobbygame->mapname, &lobbygame->maphash).c_str());
				joininggame = true;
			}
		}else
		if(world.lobby.creategamestatus != 1 && world.lobby.creategamestatus != 100 && world.lobby.creategamestatus != 0){ // failed and not creating
			world.lobby.creategamestatus = 0;
			CreateModalDialog("Could not create game");
		}
	}
	if(gameselectinterface || gamecreateinterface){
		if(joininggame){
			if(world.state == World::CONNECTED){
				joininggame = false;
			}
			if(world.state == World::IDLE){
				joininggame = false;
				CreateModalDialog("Unable to join game");
			}
		}
		if(modalinterface && !modaldialoghasok){
			Interface * modaliface = static_cast<Interface *>(world.GetObjectFromId(modalinterface));
			if(modaliface){
				for(std::vector<Uint16>::iterator it = modaliface->objects.begin(); it != modaliface->objects.end(); it++){
					Object * object = world.GetObjectFromId(*it);
					if(object && object->type == ObjectTypes::OVERLAY){
						Overlay * overlay = static_cast<Overlay *>(object);
						if(overlay->text.length() > 0){
							overlay->text = (mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 1)
								? "Uploading map" : "Creating game";
							int dots = (world.tickcount / 4) % 6;
							if(dots > 3){
								dots = 6 - dots;
							}
							for(int i = 0; i < dots; i++){
								overlay->text += ".";
							}
						}
					}
				}
			}
		}
		if(!modaldialoghasok && world.lobby.creategamestatus != 100 && mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 0 && (world.state == World::CONNECTED || world.state == World::IDLE)){
			DestroyModalDialog();
			creategameclicked = false;
		}
		if(world.state == World::CONNECTED && lobbyinterface){
			Peer * peer = world.peerlist[world.localpeerid];
			if(peer){
				if(gameselectinterface){
					Interface * lobbyiface = static_cast<Interface *>(world.GetObjectFromId(lobbyinterface));
					if(lobbyiface){
						Interface * gameselectiface = static_cast<Interface *>(world.GetObjectFromId(gameselectinterface));
						if(gameselectiface){
							gameselectiface->DestroyInterface(world, lobbyiface);
						}
					}
					gameselectinterface = 0;
				}
				if(gamecreateinterface){
					Interface * lobbyiface = static_cast<Interface *>(world.GetObjectFromId(lobbyinterface));
					if(lobbyiface){
						Interface * gamecreateiface = static_cast<Interface *>(world.GetObjectFromId(gamecreateinterface));
						if(gamecreateiface){
							gamecreateiface->DestroyInterface(world, lobbyiface);
						}
					}
					gamecreateinterface = 0;
				}
				mapDownloader.mapexistchecked = false;
				// Invalidate any in-flight server map fetch for the previous
				// game; the thread will see the stale generation and discard
				// its result. Detach so we don't block here.
				mapDownloader.mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
				mapDownloader.mapjoinstate.store(0, std::memory_order_relaxed);
				if(mapDownloader.mapjointhread.joinable()) mapDownloader.mapjointhread.detach();
				world.SetTech(Config::GetInstance().defaulttechchoices[GetSelectedAgency()]);
				gamejoininterface = CreateGameJoinInterface()->id;
				LobbyGame * lobbygame = world.lobby.GetGameById(currentlobbygameid);
				if(lobbygame){
					char temp[256];
					ambienceMixer.GetGameChannelName(*lobbygame, temp);
					strcpy(lastchannel, world.lobby.channel);
					world.lobby.JoinChannel(temp);
					UpdateLobbyMapName(lobbygame->mapname);
				}
				Interface * lobbyiface = static_cast<Interface *>(world.GetObjectFromId(lobbyinterface));
				if(lobbyiface){
					lobbyiface->AddObject(gamejoininterface);
				}
			}
		}
	}

	mapDownloader.ProcessMapDownload();

	if(world.state != World::CONNECTED && !modalinterface){
		if(gamejoininterface || gametechinterface){
			CreateModalDialog("Disconnected from game");
		}
	}
}

Interface * Game::CreateLobbyInterface(void){
	chatlinesprinted = 0;
	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 7;
	background->res_index = 1;
	Overlay * toptext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	toptext->text = "Silencer";
	toptext->textbank = 135;
	toptext->textwidth = 11;
	toptext->effectcolor = 152;
	toptext->x = 15;
	toptext->y = 32;
	Overlay * vertext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	vertext->text = "v.";
	vertext->text += world.version;
	vertext->textbank = 133;
	vertext->textwidth = 6;
	vertext->effectcolor = 189;
	vertext->x = 115;
	vertext->y = 39;
	Overlay * mapnametext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	mapnametext->textbank = 135;
	mapnametext->textwidth = 11;
	mapnametext->effectcolor = 129;
	mapnametext->effectbrightness = 128 + 32;
	mapnametext->textcolorramp = true;
	mapnametext->x = 180;
	mapnametext->y = 32;
	mapnametext->uid = 8;
	Button * exitbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	exitbutton->y = 29;
	exitbutton->x = 473;
	exitbutton->SetType(Button::B156x21);
	exitbutton->uid = 10;
	strcpy(exitbutton->text, "Go Back");
	
	characterinterface = CreateCharacterInterface()->id;
	gameselectinterface = CreateGameSelectInterface()->id;
	chatinterface = CreateChatInterface()->id;
	
	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	iface->AddObject(background->id);
	iface->AddObject(toptext->id);
	iface->AddObject(vertext->id);
	iface->AddObject(mapnametext->id);
	iface->AddObject(exitbutton->id);
	iface->AddObject(chatinterface);
	iface->AddObject(characterinterface);
	iface->AddObject(gameselectinterface);
	iface->buttonescape = exitbutton->id;
	iface->activeobject = chatinterface;
	iface->ActiveChanged(world, iface, false);
	return iface;
}

Interface * Game::CreateCharacterInterface(void){
	Interface * characterinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	characterinterface->x = 10;
	characterinterface->y = 64;
	characterinterface->width = 217;
	characterinterface->height = 120;
	Overlay * usertext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	usertext->text = world.lobby.GetLocalUsername();
	usertext->textbank = 134;
	usertext->textwidth = 8;
	usertext->effectcolor = 200;
	usertext->x = 20;
	usertext->y = 71;
	Overlay * leveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	leveltext->uid = 2;
	leveltext->textbank = 133;
	leveltext->textwidth = 7;
	leveltext->effectcolor = 129;
	leveltext->effectbrightness = 128 + 32;
	leveltext->textcolorramp = true;
	leveltext->x = 17;
	leveltext->y = 130;
	Overlay * winstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	winstext->uid = 3;
	winstext->textbank = 133;
	winstext->textwidth = 7;
	winstext->effectcolor = 129;
	winstext->effectbrightness = 128 + 32;
	winstext->textcolorramp = true;
	winstext->x = 17;
	winstext->y = 143;
	Overlay * lossestext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	lossestext->uid = 4;
	lossestext->textbank = 133;
	lossestext->textwidth = 7;
	lossestext->effectcolor = 129;
	lossestext->effectbrightness = 128 + 32;
	lossestext->textcolorramp = true;
	lossestext->x = 17;
	lossestext->y = 156;
	Overlay * etctext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	etctext->uid = 5;
	etctext->textbank = 133;
	etctext->textwidth = 7;
	etctext->effectcolor = 129;
	etctext->effectbrightness = 128 + 32;
	etctext->textcolorramp = true;
	etctext->x = 17;
	etctext->y = 169;
	int xmargin = 42;
	Toggle * noxisbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	noxisbutton->y = 90;
	noxisbutton->x = 20 + (0 * xmargin);
	noxisbutton->res_bank = 181;
	noxisbutton->res_index = 0;
	noxisbutton->uid = 1;
	noxisbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::NOXIS){
		noxisbutton->selected = true;
	}
	Toggle * lazarusbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	lazarusbutton->y = 90;
	lazarusbutton->x = 20 + (1 * xmargin);
	lazarusbutton->res_bank = 181;
	lazarusbutton->res_index = 1;
	lazarusbutton->uid = 2;
	lazarusbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::LAZARUS){
		lazarusbutton->selected = true;
	}
	Toggle * caliberbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	caliberbutton->y = 90;
	caliberbutton->x = 20 + (2 * xmargin);
	caliberbutton->res_bank = 181;
	caliberbutton->res_index = 2;
	caliberbutton->uid = 3;
	caliberbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::CALIBER){
		caliberbutton->selected = true;
	}
	Toggle * staticbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	staticbutton->y = 90;
	staticbutton->x = 20 + (3 * xmargin);
	staticbutton->res_bank = 181;
	staticbutton->res_index = 3;
	staticbutton->uid = 4;
	staticbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::STATIC){
		staticbutton->selected = true;
	}
	Toggle * blackrosebutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	blackrosebutton->y = 90;
	blackrosebutton->x = 20 + (4 * xmargin);
	blackrosebutton->res_bank = 181;
	blackrosebutton->res_index = 4;
	blackrosebutton->uid = 5;
	blackrosebutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::BLACKROSE){
		blackrosebutton->selected = true;
	}
	characterinterface->AddObject(usertext->id);
	characterinterface->AddObject(leveltext->id);
	characterinterface->AddObject(winstext->id);
	characterinterface->AddObject(lossestext->id);
	characterinterface->AddObject(etctext->id);
	characterinterface->AddObject(noxisbutton->id);
	characterinterface->AddObject(lazarusbutton->id);
	characterinterface->AddObject(caliberbutton->id);
	characterinterface->AddObject(staticbutton->id);
	characterinterface->AddObject(blackrosebutton->id);
	/*characterinterface->AddTabObject(noxisbutton->id);
	characterinterface->AddTabObject(lazarusbutton->id);
	characterinterface->AddTabObject(caliberbutton->id);
	characterinterface->AddTabObject(staticbutton->id);
	characterinterface->AddTabObject(blackrosebutton->id);*/
	return characterinterface;
}

Interface * Game::CreateGameSelectInterface(void){
	Interface * gameselectinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gameselectinterface->x = 403;
	gameselectinterface->y = 87;
	gameselectinterface->width = 222;
	gameselectinterface->height = 267;
	Overlay * rightborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	rightborder->res_bank = 7;
	rightborder->res_index = 8;
	Overlay * gamestext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamestext->text = "Active Games";
	gamestext->textbank = 134;
	gamestext->textwidth = 8;
	gamestext->x = 405;
	gamestext->y = 70;
	SelectBox * gameselect = (SelectBox *)world.CreateObject(ObjectTypes::SELECTBOX);
	gameselect->x = 407;
	gameselect->y = 89;
	gameselect->width = 214;
	gameselect->height = 265;
	gameselect->lineheight = 14;
	gameselect->uid = 10;
	ScrollBar * gamescrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	gamescrollbar->res_index = 9;
	gamescrollbar->scrollpixels = gameselect->lineheight;
	gamescrollbar->scrollposition = gameselect->scrolled;
	Overlay * gamenametext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamenametext->textbank = 133;
	gamenametext->textwidth = 6;
	gamenametext->x = 405;
	gamenametext->y = 358;
	gamenametext->uid = 1;
	Overlay * gamemaptext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamemaptext->textbank = 133;
	gamemaptext->textwidth = 6;
	gamemaptext->x = 405;
	gamemaptext->y = 370;
	gamemaptext->uid = 2;
	Overlay * gameplayerstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gameplayerstext->textbank = 133;
	gameplayerstext->textwidth = 6;
	gameplayerstext->x = 405;
	gameplayerstext->y = 382;
	gameplayerstext->uid = 3;
	Overlay * gamecreatortext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamecreatortext->textbank = 133;
	gamecreatortext->textwidth = 6;
	gamecreatortext->x = 405;
	gamecreatortext->y = 394;
	gamecreatortext->uid = 4;
	Overlay * gameinfotext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gameinfotext->textbank = 133;
	gameinfotext->textwidth = 6;
	gameinfotext->x = 405;
	gameinfotext->y = 406;
	gameinfotext->uid = 5;
	
	Button * gamejoinbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamejoinbutton->y = 430;
	gamejoinbutton->x = 436;
	gamejoinbutton->SetType(Button::B156x21);
	gamejoinbutton->uid = 20;
	strcpy(gamejoinbutton->text, "Join Game");
	Button * gamecreatebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamecreatebutton->y = 68;
	gamecreatebutton->x = 242;
	gamecreatebutton->SetType(Button::B156x21);
	gamecreatebutton->uid = 30;
	strcpy(gamecreatebutton->text, "Create Game");
	gameselectinterface->AddObject(rightborder->id);
	gameselectinterface->AddObject(gamestext->id);
	gameselectinterface->AddObject(gamejoinbutton->id);
	gameselectinterface->AddObject(gamecreatebutton->id);
	gameselectinterface->AddObject(gameselect->id);
	gameselectinterface->AddObject(gamescrollbar->id);
	gameselectinterface->AddObject(gamenametext->id);
	gameselectinterface->AddObject(gamemaptext->id);
	gameselectinterface->AddObject(gameplayerstext->id);
	gameselectinterface->AddObject(gamecreatortext->id);
	gameselectinterface->AddObject(gameinfotext->id);
	gameselectinterface->buttonenter = gamejoinbutton->id;
	gameselectinterface->scrollbar = gamescrollbar->id;
	return gameselectinterface;
}

Interface * Game::CreateChatInterface(void){
	Interface * chatinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	chatinterface->x = 15;
	chatinterface->y = 216;
	chatinterface->width = 368;
	chatinterface->height = 234;
	Overlay * chatborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	chatborder->res_bank = 7;
	chatborder->res_index = 11;
	Overlay * chatinputborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	chatinputborder->res_bank = 7;
	chatinputborder->res_index = 14;
	Overlay * channeltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	channeltext->uid = 1;
	channeltext->textbank = 134;
	channeltext->textwidth = 8;
	channeltext->x = 15;
	channeltext->y = 200;
	TextBox * textbox = (TextBox *)world.CreateObject(ObjectTypes::TEXTBOX);
	textbox->x = 19;
	textbox->y = 220;
	textbox->width = 242;
	textbox->height = 207;
	textbox->res_bank = 133;
	textbox->lineheight = 11;
	textbox->fontwidth = 6;
	textbox->bottomtotop = true;
	TextBox * presencebox = (TextBox *)world.CreateObject(ObjectTypes::TEXTBOX);
	presencebox->x = 267;
	presencebox->y = 220;
	presencebox->width = 110;
	presencebox->height = 207;
	presencebox->res_bank = 133;
	presencebox->lineheight = 11;
	presencebox->fontwidth = 6;
	presencebox->bottomtotop = false;
	presencebox->uid = 9;
	/*for(int i = 0; i < 0; i++){
		char line[256];
		sprintf(line, "line %d", i);
		textbox->AddLine(line);
	}*/
	TextInput * chatinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	chatinput->x = 18;
	chatinput->y = 437;
	chatinput->width = 360;
	chatinput->height = 14;
	chatinput->res_bank = 133;
	chatinput->fontwidth = 6;
	chatinput->maxchars = 200;
	chatinput->maxwidth = 60;
	chatinput->uid = 1;
	ScrollBar * chatscrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	chatscrollbar->res_index = 12;
	chatscrollbar->barres_index = 13;
	chatscrollbar->scrollpixels = textbox->lineheight;
	chatscrollbar->scrollposition = textbox->scrolled;
	chatinterface->AddObject(chatborder->id);
	chatinterface->AddObject(chatinputborder->id);
	chatinterface->AddObject(channeltext->id);
	chatinterface->AddObject(textbox->id);
	chatinterface->AddObject(presencebox->id);
	chatinterface->AddObject(chatinput->id);
	chatinterface->AddObject(chatscrollbar->id);
	chatinterface->AddTabObject(chatinput->id);
	chatinterface->scrollbar = chatscrollbar->id;
	//chatinterface->ActiveChanged(&world, chatinterface, false);
	return chatinterface;
}

Interface * Game::CreateGameCreateInterface(void){
	creategameclicked = false;
	mapDownloader.selectedmap = -1;
	Interface * gamecreateinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gamecreateinterface->x = 403;
	gamecreateinterface->y = 87;
	gamecreateinterface->width = 222;
	gamecreateinterface->height = 390;
	Overlay * rightborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	rightborder->res_bank = 7;
	rightborder->res_index = 8;
	
	Overlay * optionstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	optionstext->text = "Game Options";
	optionstext->textbank = 134;
	optionstext->textwidth = 8;
	optionstext->x = 272;
	optionstext->y = 70;
	
	int yoffset = 6;
	int yspace = 18;
	
	Overlay * securitytext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	securitytext->text = "Security:";
	securitytext->textbank = 134;
	securitytext->textwidth = 8;
	securitytext->x = 245;
	securitytext->y = 87 + (yspace * 0) + yoffset;
	Button * buttonsecurity = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	buttonsecurity->SetType(Button::BNONE);
	buttonsecurity->x = 323;
	buttonsecurity->y = 87 + (yspace * 0) + yoffset;
	buttonsecurity->uid = 40;
	buttonsecurity->width = 70;
	buttonsecurity->height = 20;
	buttonsecurity->textbank = 134;
	buttonsecurity->textwidth = 9;
	strcpy(buttonsecurity->text, "Medium");
	
	Overlay * minleveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	minleveltext->text = "Min Level:";
	minleveltext->textbank = 134;
	minleveltext->textwidth = 8;
	minleveltext->x = 245;
	minleveltext->y = 87 + (yspace * 1) + yoffset;
	
	TextInput * minlevelinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	minlevelinput->x = 350;
	minlevelinput->y = 87 + (yspace * 1) + yoffset;
	minlevelinput->width = 20;
	minlevelinput->height = 20;
	minlevelinput->res_bank = 134;
	minlevelinput->fontwidth = 8;
	minlevelinput->maxchars = 2;
	minlevelinput->maxwidth = 50;
	minlevelinput->uid = 41;
	minlevelinput->numbersonly = true;
	minlevelinput->SetText("0");
	
	Overlay * maxleveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxleveltext->text = "Max Level:";
	maxleveltext->textbank = 134;
	maxleveltext->textwidth = 8;
	maxleveltext->x = 245;
	maxleveltext->y = 87 + (yspace * 2) + yoffset;
	
	TextInput * maxlevelinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxlevelinput->x = 350;
	maxlevelinput->y = 87 + (yspace * 2) + yoffset;
	maxlevelinput->width = 20;
	maxlevelinput->height = 20;
	maxlevelinput->res_bank = 134;
	maxlevelinput->fontwidth = 8;
	maxlevelinput->maxchars = 2;
	maxlevelinput->maxwidth = 50;
	maxlevelinput->uid = 42;
	maxlevelinput->numbersonly = true;
	maxlevelinput->SetText("99");
	
	Overlay * maxplayerstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxplayerstext->text = "Max Players:";
	maxplayerstext->textbank = 134;
	maxplayerstext->textwidth = 8;
	maxplayerstext->x = 245;
	maxplayerstext->y = 87 + (yspace * 3) + yoffset;
	
	TextInput * maxplayersinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxplayersinput->x = 350;
	maxplayersinput->y = 87 + (yspace * 3) + yoffset;
	maxplayersinput->width = 20;
	maxplayersinput->height = 20;
	maxplayersinput->res_bank = 134;
	maxplayersinput->fontwidth = 8;
	maxplayersinput->maxchars = 2;
	maxplayersinput->maxwidth = 50;
	maxplayersinput->uid = 43;
	maxplayersinput->numbersonly = true;
	maxplayersinput->SetText("24");
	
	Overlay * maxteamstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxteamstext->text = "Max Teams:";
	maxteamstext->textbank = 134;
	maxteamstext->textwidth = 8;
	maxteamstext->x = 245;
	maxteamstext->y = 87 + (yspace * 4) + yoffset;
	
	TextInput * maxteamsinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxteamsinput->x = 350;
	maxteamsinput->y = 87 + (yspace * 4) + yoffset;
	maxteamsinput->width = 20;
	maxteamsinput->height = 20;
	maxteamsinput->res_bank = 134;
	maxteamsinput->fontwidth = 8;
	maxteamsinput->maxchars = 2;
	maxteamsinput->maxwidth = 50;
	maxteamsinput->uid = 44;
	maxteamsinput->numbersonly = true;
	maxteamsinput->SetText("6");
	
	Overlay * selectmaptext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	selectmaptext->text = "Select Map";
	selectmaptext->textbank = 134;
	selectmaptext->textwidth = 8;
	selectmaptext->x = 405;
	selectmaptext->y = 70;
	SelectBox * mapselect = (SelectBox *)world.CreateObject(ObjectTypes::SELECTBOX);
	mapselect->x = 407;
	mapselect->y = 89;
	mapselect->width = 214;
	mapselect->height = 265;
	mapselect->lineheight = 14;
	mapselect->uid = 4;
#ifdef __ANDROID__
	// need to get android directory enumeration working, hardcoded for now
	const char * maps[] = {"ALLY10c.sil", "CRAN01h.SIL", "EASY05c.SIL", "PIT16d.SIL", "STAR72.SIL", "THET06e.SIL"};
	for(int i = 0; i < sizeof(maps) / sizeof(const char *); i++){
		mapselect->AddItem(maps[i]);
	}
#else
	std::vector<std::string> maps;
	std::vector<std::string> files;
	CDResDir();
	files = mapDownloader.ListFiles((GetResDir() + "level").c_str());
	maps.insert(maps.end(), files.begin(), files.end());
	CDDataDir();
	files = mapDownloader.ListFiles((GetDataDir() + "level/download").c_str());
	for(std::vector<std::string>::iterator it = files.begin(); it != files.end(); it++){
		if(std::find(maps.begin(), maps.end(), (*it)) == maps.end()){
			maps.push_back(*it);
		}
	}
	//maps.insert(maps.end(), files.begin(), files.end());
	std::sort(maps.begin(), maps.end());
	for(std::vector<std::string>::iterator it = maps.begin(); it != maps.end(); it++){
		mapselect->AddItem((*it).c_str());
	}
	// Add server-only maps (not yet downloaded) with a download prefix
	mapDownloader.servermaps.clear();
	auto serverlist = FetchServerMapList(Config::GetInstance().mapapiurl);
	for(auto & entry : serverlist){
		bool alreadylocal = std::find(maps.begin(), maps.end(), entry.first) != maps.end();
		if(!alreadylocal){
			std::string label = "[DL] " + entry.first;
			mapselect->AddItem(label.c_str());
			mapDownloader.servermaps[label] = entry.second;
		}
	}
#endif
	mapselect->scrolled = 0;
	ScrollBar * mapscrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	mapscrollbar->res_index = 9;
	mapscrollbar->scrollpixels = mapselect->lineheight;
	mapscrollbar->scrollposition = mapselect->scrolled;
	mapscrollbar->scrollmax = mapselect->items.size();
	mapscrollbar->scrollposition = 0;
	Overlay * nametext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	nametext->text = "Game name:";
	nametext->textbank = 134;
	nametext->textwidth = 8;
	nametext->x = 405;
	nametext->y = 360;
	TextInput * nametextinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	nametextinput->x = 410;
	nametextinput->y = 375;
	nametextinput->width = 210;
	nametextinput->height = 14;
	nametextinput->res_bank = 133;
	nametextinput->fontwidth = 6;
	nametextinput->maxchars = 35;
	nametextinput->maxwidth = 35;
	nametextinput->uid = 5;
	nametextinput->SetText(Config::GetInstance().defaultgamename);
	Overlay * passwordtext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	passwordtext->text = "Password (optional):";
	passwordtext->textbank = 134;
	passwordtext->textwidth = 8;
	passwordtext->x = 405;
	passwordtext->y = 390;
	TextInput * passwordtextinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	passwordtextinput->x = 410;
	passwordtextinput->y = 405;
	passwordtextinput->width = 210;
	passwordtextinput->height = 14;
	passwordtextinput->res_bank = 133;
	passwordtextinput->fontwidth = 6;
	passwordtextinput->maxchars = 20;
	passwordtextinput->maxwidth = 20;
	passwordtextinput->uid = 6;
	passwordtextinput->password = true;
	Button * gamecreatebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamecreatebutton->y = 430;
	gamecreatebutton->x = 436;
	gamecreatebutton->SetType(Button::B156x21);
	gamecreatebutton->uid = 35;
	strcpy(gamecreatebutton->text, "Create");
	gamecreateinterface->AddObject(rightborder->id);
	gamecreateinterface->AddObject(optionstext->id);
	gamecreateinterface->AddObject(securitytext->id);
	gamecreateinterface->AddObject(buttonsecurity->id);
	gamecreateinterface->AddObject(minleveltext->id);
	gamecreateinterface->AddObject(minlevelinput->id);
	gamecreateinterface->AddObject(maxleveltext->id);
	gamecreateinterface->AddObject(maxlevelinput->id);
	gamecreateinterface->AddObject(maxplayerstext->id);
	gamecreateinterface->AddObject(maxplayersinput->id);
	gamecreateinterface->AddObject(maxteamstext->id);
	gamecreateinterface->AddObject(maxteamsinput->id);
	gamecreateinterface->AddObject(selectmaptext->id);
	gamecreateinterface->AddObject(mapselect->id);
	gamecreateinterface->AddObject(mapscrollbar->id);
	gamecreateinterface->AddObject(nametext->id);
	gamecreateinterface->AddObject(nametextinput->id);
	gamecreateinterface->AddObject(passwordtext->id);
	gamecreateinterface->AddObject(passwordtextinput->id);
	gamecreateinterface->AddObject(gamecreatebutton->id);
	//gamecreateinterface->AddTabObject(buttonsecurity->id);
	gamecreateinterface->AddTabObject(nametextinput->id);
	gamecreateinterface->AddTabObject(passwordtextinput->id);
	gamecreateinterface->AddTabObject(gamecreatebutton->id);
	gamecreateinterface->AddTabObject(minlevelinput->id);
	gamecreateinterface->AddTabObject(maxlevelinput->id);
	gamecreateinterface->AddTabObject(maxplayersinput->id);
	gamecreateinterface->AddTabObject(maxteamsinput->id);
	gamecreateinterface->scrollbar = mapscrollbar->id;
	gamecreateinterface->buttonenter = gamecreatebutton->id;
	gamecreateinterface->activeobject = nametextinput->id;
	return gamecreateinterface;
}

Interface * Game::CreateGameJoinInterface(void){
	Interface * gamejoininterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gamejoininterface->x = 403;
	gamejoininterface->y = 87;
	gamejoininterface->width = 222;
	gamejoininterface->height = 267;
	Button * gamestartbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamestartbutton->x = 242;
	gamestartbutton->y = 160;
	gamestartbutton->SetType(Button::B156x21);
	gamestartbutton->uid = 25;
	//if(ishost){
	//	strcpy(gamestartbutton->text, "Start Game");
	//}else{
		strcpy(gamestartbutton->text, "Ready");
	//}
	Button * techbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	techbutton->x = 242;
	techbutton->y = 68;
	techbutton->SetType(Button::B156x21);
	techbutton->uid = 27;
	strcpy(techbutton->text, "Choose Tech");
	Button * changeteambutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	changeteambutton->x = 242;
	changeteambutton->y = 100;
	changeteambutton->SetType(Button::B156x21);
	changeteambutton->uid = 26;
	strcpy(changeteambutton->text, "Change Team");
	gamejoininterface->AddObject(techbutton->id);
	gamejoininterface->AddObject(changeteambutton->id);
	gamejoininterface->AddObject(gamestartbutton->id);
	gamejoininterface->buttonenter = gamestartbutton->id;
	return gamejoininterface;
}

Interface * Game::CreateGameTechInterface(void){
	Interface * gametechinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gametechinterface->x = 403;
	gametechinterface->y = 87;
	gametechinterface->width = 222;
	gametechinterface->height = 390;
	Button * teamsbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	teamsbutton->x = 242;
	teamsbutton->y = 68;
	teamsbutton->SetType(Button::B156x21);
	teamsbutton->uid = 28;
	strcpy(teamsbutton->text, "Back To Teams");
	Overlay * techslotsoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	if(techslotsoverlay){
		techslotsoverlay->x = 455;
		techslotsoverlay->y = 100;
		techslotsoverlay->textbank = 133;
		techslotsoverlay->textwidth = 6;
		techslotsoverlay->effectcolor = 129;
		techslotsoverlay->effectbrightness = 128 + 16;
		techslotsoverlay->textcolorramp = true;
		techslotsoverlay->uid = 70;
		gametechinterface->AddObject(techslotsoverlay->id);
	}
	for(int i = 0; i < 3; i++){
		int j = 2 - i;
		Overlay * techoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		if(techoverlay){
			techoverlay->text = "Player ";
			techoverlay->text += std::to_string(i + 1);
			techoverlay->textbank = 133;
			techoverlay->textwidth = 6;
			techoverlay->uid = 80 + i;
			techoverlay->x = 375 - (techoverlay->text.length() * techoverlay->textwidth);
			techoverlay->y = 112 + (j * 16);
			techoverlay->draw = false;
			gametechinterface->AddObject(techoverlay->id);
		}
		Overlay * techlineoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		if(techlineoverlay){
			techlineoverlay->x = 0;
			techlineoverlay->y = 0;
			techlineoverlay->uid = 90 + i;
			techlineoverlay->res_bank = 7;
			techlineoverlay->res_index = 20 + j;
			techlineoverlay->draw = false;
			gametechinterface->AddObject(techlineoverlay->id);
		}
	}
	Team * team = world.GetPeerTeam(world.localpeerid);
	if(team){
		for(int x = 0; x < 4; x++){
			int i = 0;
			int ipos = 0;
			for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
				BuyableItem * buyableitem = *it;
				if(buyableitem->techslots){
					if(buyableitem->agencyspecific == -1 || (buyableitem->agencyspecific == team->agency)){
						Button * button = (Button *)world.CreateObject(ObjectTypes::BUTTON);
						if(button){
							button->x = 410 + (x * 14);
							button->y = 125 + (ipos * 13);
							button->uid = 110 + (30 * x) + i;
							button->SetType(Button::BCHECKBOX);
							if(x < 3){
								button->effectbrightness = 64;
								button->draw = false;
							}
							gametechinterface->AddObject(button->id);
							if(x == 3){
								Overlay * technameoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
								if(technameoverlay){
									technameoverlay->text = buyableitem->name;
									technameoverlay->text += " (" + std::to_string(buyableitem->techslots) + ")";
									technameoverlay->x = 425 + (x * 14);
									technameoverlay->y = 127 + (ipos * 13);
									technameoverlay->textbank = 133;
									technameoverlay->textwidth = 6;
									technameoverlay->uid = 230 + i;
									gametechinterface->AddObject(technameoverlay->id);
								}
							}
						}
						ipos++;
					}
					i++;
				}
			}
		}
	}
	Overlay * techname = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	if(techname){
		techname->textbank = 134;
		techname->textwidth = 8;
		techname->uid = 60;
		techname->x = 401 + (116 - ((techname->text.length() * techname->textwidth) / 2));
		techname->y = 350;
		gametechinterface->AddObject(techname->id);
	}
	for(int i = 0; i < 8; i++){
		Overlay * techdesc = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		if(techdesc){
			techdesc->textbank = 133;
			techdesc->textwidth = 6;
			techdesc->effectcolor = 129;
			techdesc->effectbrightness = 128 + 16;
			techdesc->textcolorramp = true;
			techdesc->uid = 61 + i;
			techdesc->x = 405;
			techdesc->y = 370 + (i * 10);
			gametechinterface->AddObject(techdesc->id);
		}
	}
	gametechinterface->AddObject(teamsbutton->id);
	gametechinterface->buttonescape = teamsbutton->id;
	return gametechinterface;
}

Interface * Game::CreateGameSummaryInterface(Stats & stats, Uint8 agency){
	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 6;
	background->res_index = 0;
	Overlay * background2 = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background2->res_bank = 7;
	background2->res_index = 5;
	Overlay * title = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	title->text = "Mission Summary";
	title->textbank = 135;
	title->textwidth = 12;
	title->x = 192 - ((title->text.length() * title->textwidth) / 2);
	title->y = 44;
	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	TextBox * textbox = (TextBox *)world.CreateObject(ObjectTypes::TEXTBOX);
	textbox->x = 89;
	textbox->y = 92;
	textbox->width = 180;
	textbox->height = 300;
	textbox->res_bank = 133;
	textbox->lineheight = 11;
	textbox->fontwidth = 6;
	
	AddSummaryLine(*textbox, "Kills:", stats.kills);
	AddSummaryLine(*textbox, "Deaths:", stats.deaths);
	AddSummaryLine(*textbox, "Suicides", stats.suicides);
	textbox->AddLine("");
	textbox->AddLine("Secrets");
	AddSummaryLine(*textbox, "  Returned:", stats.secretsreturned);
	AddSummaryLine(*textbox, "  Stolen:", stats.secretsstolen);
	AddSummaryLine(*textbox, "  Picked up:", stats.secretspickedup);
	AddSummaryLine(*textbox, "  Fumbled:", stats.secretsdropped);
	textbox->AddLine("");
	AddSummaryLine(*textbox, "Civilians killed:", stats.civilianskilled);
	AddSummaryLine(*textbox, "Guards killed:", stats.guardskilled);
	AddSummaryLine(*textbox, "Robots killed:", stats.robotskilled);
	AddSummaryLine(*textbox, "Defenses destroyed:", stats.defensekilled);
	AddSummaryLine(*textbox, "Fixed Cannons destroyed:", stats.fixedcannonsdestroyed);
	textbox->AddLine("");
	textbox->AddLine("Files");
	AddSummaryLine(*textbox, "  Hacked:", stats.fileshacked);
	AddSummaryLine(*textbox, "  Returned:", stats.filesreturned);
	textbox->AddLine("");
	AddSummaryLine(*textbox, "Powerups picked up:", stats.powerupspickedup);
	AddSummaryLine(*textbox, "Health packs used:", stats.healthpacksused);
	AddSummaryLine(*textbox, "Cameras placed:", stats.camerasplanted);
	AddSummaryLine(*textbox, "Detonators planted:", stats.detsplanted);
	AddSummaryLine(*textbox, "Fixed Cannons placed:", stats.fixedcannonsplaced);
	AddSummaryLine(*textbox, "Viruses used:", stats.virusesused);
	AddSummaryLine(*textbox, "Poisons:", stats.poisons);
	AddSummaryLine(*textbox, "Lazarus Tracts planted:", stats.tractsplanted);
	textbox->AddLine("");
	textbox->AddLine("Grenades thrown");
	AddSummaryLine(*textbox, "  E.M.P:", stats.empsthrown);
	AddSummaryLine(*textbox, "  Plasma:", stats.plasmasthrown);
	AddSummaryLine(*textbox, "  Shaped:", stats.shapedthrown);
	AddSummaryLine(*textbox, "  Flare:", stats.flaresthrown);
	AddSummaryLine(*textbox, "  Poison Flare:", stats.poisonflaresthrown);
	AddSummaryLine(*textbox, "  Neutron:", stats.neutronsthrown);
	for(int i = 0; i < 4; i++){
		textbox->AddLine("");
		switch(i){
			case 0: textbox->AddLine("Blaster"); break;
			case 1: textbox->AddLine("Laser"); break;
			case 2: textbox->AddLine("Rocket"); break;
			case 3: textbox->AddLine("Flamer"); break;
		}
		AddSummaryLine(*textbox, "  Shots fired:", stats.weaponfires[i]);
		AddSummaryLine(*textbox, "  Hits:", stats.weaponhits[i]);
		AddSummaryLine(*textbox, "  Accuracy:", (float(stats.weaponhits[i]) / stats.weaponfires[i]) * 100, true);
		AddSummaryLine(*textbox, "  Player kills:", stats.playerkillsweapon[i]);
	}
	ScrollBar * scrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	scrollbar->res_index = 9;
	scrollbar->scrollposition = 0;
	scrollbar->scrollmax = textbox->text.size() - (textbox->height / textbox->lineheight);
	scrollbar->scrollpixels = textbox->lineheight;
	scrollbar->scrollposition = 0;
	
	Overlay * xptext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	xptext->text = "+ ";
	xptext->text += std::to_string(stats.CalculateExperience()) + " XP";
	xptext->textbank = 136;
	xptext->textwidth = 15;
	xptext->x = 467 - ((xptext->text.length() * xptext->textwidth) / 2);
	xptext->y = 45;
	
	//bool upgradeavailable = true;
	
	Overlay * line1text = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	line1text->text = "*NEW UPGRADE AVAILABLE*";
	line1text->textbank = 133;
	line1text->effectcolor = 129;
	line1text->effectbrightness = 128 + 32;
	line1text->textcolorramp = true;
	line1text->textwidth = 6;
	line1text->uid = 1;
	line1text->x = 467 - ((line1text->text.length() * line1text->textwidth) / 2);
	line1text->y = 77;
	line1text->draw = false;
	iface->AddObject(line1text->id);
	/*if(upgradeavailable){
		line1text->draw = true;
	}else{
		line1text->draw = false;
	}*/
	
	for(int i = 0; i < 6; i++){
		Overlay * text = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		Overlay * textlevel = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		switch(i){
			default:
			case 0: text->text = "Current Endurance Level:"; break;
			case 1: text->text = "Current Shield Level:"; break;
			case 2: text->text = "Current Jetpack Level:"; break;
			case 3: text->text = "Current Tech Slot Level:"; break;
			case 4: text->text = "Current Hacking Level:"; break;
			case 5: text->text = "Current Contacts Level:"; break;
		}
		text->textbank = 133;
		textlevel->textbank = 133;
		text->textwidth = 6;
		textlevel->textwidth = 6;
		textlevel->uid = 20 + i;
		text->x = 390;
		textlevel->x = 556 - (textlevel->text.length() * textlevel->textwidth);
		text->y = 97 + (i * 46);
		textlevel->y = text->y;
		iface->AddObject(text->id);
		iface->AddObject(textlevel->id);
	}
	
	Button * okbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	okbutton->y = 100;
	okbutton->x = 62;
	okbutton->uid = 0;
	strcpy(okbutton->text, "Done");
	iface->AddObject(background->id);
	iface->AddObject(background2->id);
	iface->AddObject(title->id);
	iface->AddObject(textbox->id);
	iface->AddObject(scrollbar->id);
	iface->AddObject(okbutton->id);
	iface->buttonenter = okbutton->id;
	iface->buttonescape = okbutton->id;
	iface->scrollbar = scrollbar->id;
	gamesummaryinterface = iface->id;
	UpdateGameSummaryInterface();
	return iface;
}

Interface * Game::CreateModalDialog(const char * message, bool ok){
	DestroyModalDialog();
	// 40:4 model dialog background
	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->renderpass = 3;
	background->res_bank = 40;
	background->res_index = 4;
	Overlay * text = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	text->renderpass = 3;
	text->text = message;
	text->textbank = 134;
	text->textwidth = 8;
	text->x = 320 - ((text->text.length() * text->textwidth) / 2);
	text->y = 200;
	Interface * dialoginterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	if(ok){
		Button * okbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
		okbutton->renderpass = 3;
		okbutton->x = 242;
		okbutton->y = 230;
		okbutton->SetType(Button::B156x21);
		okbutton->uid = 50;
		strcpy(okbutton->text, "OK");
		dialoginterface->AddObject(okbutton->id);
		dialoginterface->buttonenter = okbutton->id;
	}else{
		text->y = 218;
	}
	modaldialoghasok = ok;
	dialoginterface->AddObject(background->id);
	dialoginterface->AddObject(text->id);
	dialoginterface->modal = true;
	modalinterface = dialoginterface->id;
	Interface * iface = static_cast<Interface *>(world.GetObjectFromId(currentinterface));
	if(iface){
		iface->AddObject(modalinterface);
	}
	aftermodalinterface = currentinterface;
	currentinterface = dialoginterface->id;
	return dialoginterface;
}

Interface * Game::CreateMapPreview(const char * filename){
	Interface * previewinterface = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	Overlay * minimap = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	minimap->customsprite.resize(172 * 62);
	minimap->uid = 1;
	memset(minimap->customsprite.data(), 0, minimap->customsprite.size());
	Overlay * mapname = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	mapname->textbank = 133;
	mapname->textwidth = 7;
	mapname->effectcolor = 129;
	mapname->effectbrightness = 128 + 32;
	mapname->textcolorramp = true;
	mapname->uid = 2;
	std::string smallfilename = filename;
	int lastslash = smallfilename.find_last_of("/");
	if(lastslash){
		smallfilename.erase(0, lastslash + 1);
	}
	mapname->text = smallfilename.substr(0, 29);
	Overlay * maptext = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	maptext->textbank = 133;
	maptext->textwidth = 7;
	maptext->textallownewline = true;
	maptext->textlineheight = 10;
	maptext->effectcolor = 129;
	maptext->effectbrightness = 128 + 32;
	maptext->textcolorramp = true;
	maptext->uid = 3;
	char mapdesc[0x80];
	strcpy(mapdesc, "");
	CDDataDir();
	SDL_IOStream * file = SDL_IOFromFile(filename, "rb");
	if(!file){
		CDResDir();
		file = SDL_IOFromFile(filename, "rb");
	}
	if(file){
		Map::Header header;
		Map::LoadHeader(file, header);
		strcpy(mapdesc, header.description);
		Map::UncompressMinimap((Uint8 (*)[172 * 62])minimap->customsprite.data(), header.minimapcompressed, header.minimapcompressedsize);
		minimap->customspritew = 172;
		minimap->customspriteh = 62;
		SDL_CloseIO(file);
	}
	maptext->text = Interface::WordWrap(mapdesc, 29);
	previewinterface->AddObject(mapname->id);
	previewinterface->AddObject(minimap->id);
	previewinterface->AddObject(maptext->id);
	return previewinterface;
}

void Game::DestroyModalDialog(void){
	if(modalinterface){
		currentinterface = aftermodalinterface;
		aftermodalinterface = 0;
		Interface * iface = static_cast<Interface *>(world.GetObjectFromId(currentinterface));
		if(iface){
			Interface * modaliface = static_cast<Interface *>(world.GetObjectFromId(modalinterface));
			if(modaliface){
				modaliface->DestroyInterface(world, iface);
			}
		}
		modalinterface = 0;
	}
}

Interface * Game::CreatePasswordDialog(void){
	// 40:2 model dialog password input
	DestroyModalDialog();
	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->renderpass = 3;
	background->res_bank = 40;
	background->res_index = 2;
	background->x = 320 - (world.resources.spritewidth[background->res_bank][background->res_index] / 2);
	background->y = 240 - (world.resources.spriteheight[background->res_bank][background->res_index] / 2);
	Interface * dialoginterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	Overlay * text = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	text->renderpass = 3;
	text->text = "This game requires a password";
	text->textbank = 134;
	text->textwidth = 8;
	text->x = 320 - ((text->text.length() * text->textwidth) / 2);
	text->y = 196;
	TextInput * passwordinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	passwordinput->renderpass = 3;
	passwordinput->x = 210;
	passwordinput->y = 243;
	passwordinput->width = 180;
	passwordinput->height = 14;
	passwordinput->res_bank = 135;
	passwordinput->fontwidth = 11;
	passwordinput->maxchars = 20;
	passwordinput->maxwidth = 20;
	passwordinput->password = true;
	passwordinput->uid = 1;
	modaldialoghasok = true;
	Button * okbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	okbutton->renderpass = 3;
	okbutton->x = 242;
	okbutton->y = 267;
	okbutton->SetType(Button::B156x21);
	okbutton->uid = 50;
	strcpy(okbutton->text, "OK");
	dialoginterface->AddObject(background->id);
	dialoginterface->AddObject(text->id);
	dialoginterface->AddObject(passwordinput->id);
	dialoginterface->AddObject(okbutton->id);
	dialoginterface->AddTabObject(passwordinput->id);
	dialoginterface->AddTabObject(okbutton->id);
	dialoginterface->activeobject = passwordinput->id;
	dialoginterface->buttonenter = okbutton->id;
	dialoginterface->buttonescape = okbutton->id;
	dialoginterface->modal = true;
	modalinterface = dialoginterface->id;
	aftermodalinterface = currentinterface;
	Interface * iface = static_cast<Interface *>(world.GetObjectFromId(currentinterface));
	if(iface){
		iface->AddObject(modalinterface);
	}
	currentinterface = dialoginterface->id;
	passwordinterface = dialoginterface->id;
	return dialoginterface;
}

bool Game::GoBack(void){
	if(gamejoininterface || gametechinterface){
		world.Disconnect();
		UpdateLobbyMapName("");
		world.lobby.gamesprocessed = false;
		world.lobby.channelchanged = true;
		world.SwitchToLocalAuthorityMode();
		sharedstate = 0;
		Object * object = world.GetObjectFromId(currentinterface);
		Interface * iface = static_cast<Interface *>(object);
		if(gamejoininterface){
			Interface * gamejoiniface = static_cast<Interface *>(world.GetObjectFromId(gamejoininterface));
			if(gamejoiniface){
				gamejoiniface->DestroyInterface(world, iface);
			}
			gamejoininterface = 0;
			for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
				Object * object = *it;
				switch(object->type){
					case ObjectTypes::TEAM:{
						Team * team = static_cast<Team *>(object);
						team->DestroyOverlays(world);
						world.MarkDestroyObject(object->id);
					}break;
				}
			}
		}else
		if(gametechinterface){
			Interface * gametechiface = static_cast<Interface *>(world.GetObjectFromId(gametechinterface));
			if(gametechiface){
				gametechiface->DestroyInterface(world, iface);
			}
			gametechinterface = 0;
			world.choosingtech = false;
			for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
				Object * object = *it;
				switch(object->type){
					case ObjectTypes::TEAM:{
						Team * team = static_cast<Team *>(object);
						team->DestroyOverlays(world);
						world.MarkDestroyObject(object->id);
					}break;
				}
			}
		}
		gameselectinterface = CreateGameSelectInterface()->id;
		world.lobby.JoinChannel(lastchannel);
		iface->AddObject(gameselectinterface);
		currentinterface = iface->id;
		return true;
	}else
	if(gamecreateinterface){
		Object * object = world.GetObjectFromId(currentinterface);
		Interface * iface = static_cast<Interface *>(object);
		Interface * gamecreateiface = static_cast<Interface *>(world.GetObjectFromId(gamecreateinterface));
		if(gamecreateiface){
			gamecreateiface->DestroyInterface(world, iface);
		}
		gamecreateinterface = 0;
		gameselectinterface = CreateGameSelectInterface()->id;
		world.lobby.gamesprocessed = false;
		iface->AddObject(gameselectinterface);
		currentinterface = iface->id;
		return true;
	}else{
		GoToState(MAINMENU);
	}
	return false;
}

bool Game::ProcessLobbyInterface(Interface * iface){
	UpdateTechInterface();
	if(mappreviewinterface && !gamecreateinterface){
		Interface * mappreviewiface = static_cast<Interface *>(world.GetObjectFromId(mappreviewinterface));
		if(mappreviewiface){
			mappreviewiface->DestroyInterface(world);
		}
		mappreviewinterface = 0;
	}
	for(int i = 0; i < iface->objects.size(); i++){
		if(i >= iface->objects.size()){
			return false;
		}
		Uint16 id = iface->objects[i];
		Object * object = world.GetObjectFromId(id);
		if(object){
			switch(object->type){
				case ObjectTypes::SCROLLBAR:{
					ScrollBar * scrollbar = static_cast<ScrollBar *>(object);
					if(scrollbar){
						
					}
				}break;
				case ObjectTypes::INTERFACE:{
					Interface * iface = static_cast<Interface *>(object);
					if(iface){
						if(!ProcessLobbyInterface(iface)){
							return false;
						}
					}
				}break;
				case ObjectTypes::TEXTINPUT:{
					TextInput * textinput = static_cast<TextInput *>(object);
					if(textinput){
						Interface * chatiface = static_cast<Interface *>(world.GetObjectFromId(chatinterface));
						if(chatiface && iface->activeobject == chatiface->activeobject){
							if(textinput->enterpressed && strlen(textinput->text) > 0){
								world.lobby.SendChat(world.lobby.channel, textinput->text);
								textinput->Clear();
							}
						}
						if(textinput->enterpressed){
							textinput->enterpressed = false;
						}
					}
				}break;
				case ObjectTypes::SELECTBOX:{
					SelectBox * selectbox = static_cast<SelectBox *>(object);
					if(selectbox){
						Object * object = world.GetObjectFromId(iface->scrollbar);
						ScrollBar * scrollbar = static_cast<ScrollBar *>(object);
						if(scrollbar){
							selectbox->scrolled = scrollbar->scrollposition;
							if(selectbox->items.size() > ceil(float(selectbox->height) / selectbox->lineheight)){
								scrollbar->draw = true;
								scrollbar->scrollmax = selectbox->items.size() - ceil(float(selectbox->height) / selectbox->lineheight);
							}else{
								scrollbar->draw = false;
							}
							if(selectbox->uid == 4){ // map select
								// Poll async download completion each frame
								if(!mapDownloader.dlitemname.empty()){
									int res = mapDownloader.dlresult.load();
									if(res == 1){ // success — rebuild interface the same way the Create button does
										selectbox->downloadprogress = -1;
										selectbox->downloaditem[0] = '\0';
										mapDownloader.servermaps.erase(mapDownloader.dlitemname);
										mapDownloader.dlitemname.clear();
										Interface * parentIface = static_cast<Interface *>(world.GetObjectFromId(currentinterface));
										Interface * old_create = static_cast<Interface *>(world.GetObjectFromId(gamecreateinterface));
										if(old_create && parentIface) old_create->DestroyInterface(world, parentIface);
										gamecreateinterface = CreateGameCreateInterface()->id;
										if(parentIface){
											parentIface->AddObject(gamecreateinterface);
											parentIface->activeobject = gamecreateinterface;
											parentIface->ActiveChanged(world, parentIface, false);
										}
										return false;
									}else if(res == -1){ // failed
										selectbox->downloadprogress = -1;
										selectbox->downloaditem[0] = '\0';
										mapDownloader.dlitemname.clear();
										CreateModalDialog("Download failed");
									}else{ // still in progress — update progress bar
										selectbox->downloadprogress = mapDownloader.dlprogress.load();
									}
								}
								int index = selectbox->MouseInside(world, iface->mousex, iface->mousey);
								// DL badge click: raw coord check (badge lives in the 16px right margin MouseInside excludes)
								if(iface->mousedown && mapDownloader.dlitemname.empty() &&
								   iface->mousex >= selectbox->x + selectbox->width - 16 &&
								   iface->mousey >= selectbox->y && iface->mousey < selectbox->y + selectbox->height){
									int row = (iface->mousey - selectbox->y) / selectbox->lineheight + selectbox->scrolled;
									if(row >= 0 && row < (int)selectbox->items.size()){
										std::string clickeditem = selectbox->GetItemName(row);
										auto dlit = mapDownloader.servermaps.find(clickeditem);
										if(dlit != mapDownloader.servermaps.end()){
											const std::string & hex = dlit->second;
											unsigned char sha1bytes[20] = {};
											bool ok = hex.size() == 40;
											for(int j = 0; ok && j < 20; j++){
												auto hv = [](char c) -> int {
													if(c >= '0' && c <= '9') return c - '0';
													if(c >= 'a' && c <= 'f') return c - 'a' + 10;
													if(c >= 'A' && c <= 'F') return c - 'A' + 10;
													return -1;
												};
												int hi = hv(hex[2*j]), lo = hv(hex[2*j+1]);
												if(hi < 0 || lo < 0){ ok = false; break; }
												sha1bytes[j] = (unsigned char)((hi << 4) | lo);
											}
											if(ok){
												std::string dlname = clickeditem.substr(5);
												mapDownloader.dlitemname = clickeditem;
												mapDownloader.dlresult.store(0);
												mapDownloader.dlprogress.store(0);
												snprintf(selectbox->downloaditem, sizeof(selectbox->downloaditem), "%s", dlname.c_str());
												selectbox->downloadprogress = 0;
												if(mapDownloader.dlthread.joinable()) mapDownloader.dlthread.join();
												std::string apiURL = Config::GetInstance().mapapiurl;
												mapDownloader.dlthread = std::thread([this, dlname, sha1bytes, apiURL]() mutable {
													std::string res = FetchMapFromServer(dlname.c_str(), sha1bytes, apiURL.c_str(), &mapDownloader.dlprogress);
													mapDownloader.dlresult.store(res.empty() ? -1 : 1);
												});
											}
										}
									}
								}
								if(index != -1){
									std::string itemname = selectbox->GetItemName(index);
									bool isserver = mapDownloader.servermaps.count(itemname) > 0;
									if(index != mapDownloader.selectedmap){
										if(mappreviewinterface){
											Interface * mappreviewiface = static_cast<Interface *>(world.GetObjectFromId(mappreviewinterface));
											if(mappreviewiface){
												mappreviewiface->DestroyInterface(world);
											}
											mappreviewinterface = 0;
										}
										if(!isserver){
											std::string filename = mapDownloader.FindMap(itemname.c_str());
											mappreviewinterface = CreateMapPreview(filename.c_str())->id;
										}
										mapDownloader.selectedmap = index;
									}
									if(!isserver && mappreviewinterface){
										Interface * mappreviewiface = static_cast<Interface *>(world.GetObjectFromId(mappreviewinterface));
										if(mappreviewiface){
											for(std::vector<Uint16>::iterator it2 = mappreviewiface->objects.begin(); it2 != mappreviewiface->objects.end(); it2++){
												Object * object = world.GetObjectFromId(*it2);
												switch(object->type){
													case ObjectTypes::OVERLAY:{
														Overlay * overlay = static_cast<Overlay *>(object);
														switch(overlay->uid){
															case 1:
																overlay->x = iface->mousex - 200 + 15;
																overlay->y = iface->mousey - 30;
															break;
															case 2:
																overlay->x = iface->mousex - 200 + 2;
																overlay->y = iface->mousey - 30 - 7 - 5;
															break;
															case 3:
																overlay->x = iface->mousex - 200 + 2;
																overlay->y = iface->mousey - 30 + 62 + 5;
															break;
														}
													}break;
												}
											}
										}
									}
								}else{
									Interface * mappreviewiface = static_cast<Interface *>(world.GetObjectFromId(mappreviewinterface));
									if(mappreviewiface){
										mappreviewiface->DestroyInterface(world);
									}
									mapDownloader.selectedmap = -1;
									mappreviewinterface = 0;
								}
							}else
							if(selectbox->uid == 10){ // game select
								if(!world.lobby.gamesprocessed){
									bool deleted;
									do{
										deleted = false;
										unsigned int index = 0;
										for(std::deque<Uint32>::iterator it2 = selectbox->itemids.begin(); it2 != selectbox->itemids.end(); it2++, index++){
											Uint32 gameid = (*it2);
											if(!world.lobby.GetGameById(gameid)){
												selectbox->DeleteItem(index);
												deleted = true;
												break;
											}
										}
									}while(deleted);
									for(std::list<LobbyGame *>::iterator it2 = world.lobby.games.begin(); it2 != world.lobby.games.end(); it2++){
										LobbyGame * lobbygame = (*it2);
										if(selectbox->IdToIndex(lobbygame->id) == -1){
											selectbox->AddItem(lobbygame->name, lobbygame->id);
										}
									}
									world.lobby.gamesprocessed = true;
								}
								
								LobbyGame * lobbygame = world.lobby.GetGameById(selectbox->IndexToId(selectbox->selecteditem));
								Object * tobject = iface->GetObjectWithUid(world, 1);
								if(tobject && tobject->type == ObjectTypes::OVERLAY){
									Overlay * overlay = static_cast<Overlay *>(tobject);
									if(overlay){
										if(lobbygame){
											overlay->text = lobbygame->name;
										}else{
											overlay->text = "";
										}
									}
								}
								tobject = iface->GetObjectWithUid(world, 2);
								if(tobject && tobject->type == ObjectTypes::OVERLAY){
									Overlay * overlay = static_cast<Overlay *>(tobject);
									if(overlay){
										if(lobbygame){
											overlay->text = "Map: ";
											overlay->text += lobbygame->mapname;
										}else{
											overlay->text = "";
										}
									}
								}
								tobject = iface->GetObjectWithUid(world, 3);
								if(tobject && tobject->type == ObjectTypes::OVERLAY){
									Overlay * overlay = static_cast<Overlay *>(tobject);
									if(overlay){
										if(lobbygame){
											const char * passwordlock = "";
											if(strlen(lobbygame->password) > 0){
												passwordlock = "*PASSWORD LOCK*";
											}
											std::string security = "No";
											switch(lobbygame->securitylevel){
												case LobbyGame::SECLOW:
													security = "Low";
													break;
												case LobbyGame::SECMEDIUM:
													security = "Medium";
													break;
												case LobbyGame::SECHIGH:
													security = "High";
													break;
											}
											overlay->text = security + " Security";
											while(overlay->text.length() < 21){
												overlay->text += " ";
											}
											overlay->text += passwordlock;
										}else{
											overlay->text = "";
										}
									}
								}
								tobject = iface->GetObjectWithUid(world, 4);
								if(tobject && tobject->type == ObjectTypes::OVERLAY){
									Overlay * overlay = static_cast<Overlay *>(tobject);
									if(overlay){
										if(lobbygame){
											overlay->text = "Creator: ";
											overlay->text += world.lobby.GetUserInfo(lobbygame->accountid)->name;
										}else{
											overlay->text = "";
										}
									}
								}
								tobject = iface->GetObjectWithUid(world, 5);
								if(tobject && tobject->type == ObjectTypes::OVERLAY){
									Overlay * overlay = static_cast<Overlay *>(tobject);
									if(overlay){
										if(lobbygame){
											overlay->text = "MinLv:" + std::to_string(lobbygame->minlevel) + " MaxLv:" + std::to_string(lobbygame->maxlevel) + " MaxPl:" + std::to_string(lobbygame->maxplayers) + " MaxTm:" + std::to_string(lobbygame->maxteams);
										}else{
											overlay->text = "";
										}
									}
								}
							}
						}
					}
				}break;
				case ObjectTypes::TOGGLE:{
					if(iface->id == characterinterface){
						Uint8 selectedagency = GetSelectedAgency();
						if(selectedagency != oldselectedagency){
							Config::GetInstance().defaultagency = selectedagency;
							Config::GetInstance().Save();
							oldselectedagency = selectedagency;
							agencychanged = true;
							if(world.state == World::CONNECTED){
								world.SetAgency(selectedagency);
							}
						}
					}
				}break;
				case ObjectTypes::TEXTBOX:{
					TextBox * textbox = static_cast<TextBox *>(object);
					if(textbox && textbox->uid == 9){
						if(world.lobby.presencechanged || !world.lobby.gamesprocessed){
							textbox->text.clear();
							textbox->scrolled = 0;
							struct Row { Uint8 group; std::string label; };
							std::vector<Row> rows;
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
								rows.push_back(r);
							}
							std::sort(rows.begin(), rows.end(), [](const Row & a, const Row & b){
								if(a.group != b.group) return a.group < b.group;
								return a.label < b.label;
							});
							Uint8 lastgroup = 255;
							for(auto & r : rows){
								if(r.group != lastgroup){
									const char * header = (r.group == 0) ? "In Lobby" : (r.group == 1) ? "Pregame" : "Playing";
									textbox->AddText(header, 0, 128 + 32, 0, false);
									lastgroup = r.group;
								}
								textbox->AddText(r.label.c_str(), 0, 128, 2, false);
							}
							world.lobby.presencechanged = false;
						}
					}else
					if(textbox){
						Object * object = world.GetObjectFromId(iface->scrollbar);
						ScrollBar * scrollbar = static_cast<ScrollBar *>(object);
						if(minimized && world.lobby.chatmessages.size() > chatlinesprinted){
#ifdef _WIN32
							HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
							if(hwnd){
								FLASHWINFO flashinfo;
								flashinfo.cbSize = sizeof(flashinfo);
								flashinfo.hwnd = hwnd;
								flashinfo.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
								flashinfo.uCount = 0xFFFFFFFF;
								flashinfo.dwTimeout = 0;
								FlashWindowEx(&flashinfo);
							}
#endif
#ifdef __APPLE__
							RequestUserAttention();
#endif
						}
						bool scroll = false;
						while(world.lobby.chatmessages.size() > chatlinesprinted){
							auto message = world.lobby.chatmessages.front();
							Uint8 color = message[strlen(message.data()) + 1];
							Uint8 brightness = message[strlen(message.data()) + 2];
							if(scrollbar && scrollbar->scrollposition == scrollbar->scrollmax){
								scroll = true;
							}
							textbox->AddText(message.data(), color, brightness, 2, scroll);
							world.lobby.chatmessages.pop_front();
							if(scrollbar){
								scrollbar->scrollposition = textbox->scrolled;
							}
						}
						if(scrollbar){
							textbox->scrolled = scrollbar->scrollposition;
							if(textbox->text.size() > ceil(float(textbox->height) / textbox->lineheight)){
								scrollbar->draw = true;
								scrollbar->scrollmax = textbox->text.size() - ceil(float(textbox->height) / textbox->lineheight);
							}else{
								scrollbar->draw = false;
							}
						}
					}
				}break;
				case ObjectTypes::OVERLAY:{
					Overlay * overlay = static_cast<Overlay *>(object);
					if(overlay){
						switch(overlay->uid){
							case 1:{
								if(world.lobby.channelchanged){
									if(strlen(lastchannel) == 0){
										strcpy(lastchannel, world.lobby.channel);
									}
									overlay->text = world.lobby.channel;
									world.lobby.channelchanged = false;
								}
							}break;
						}
						if(agencychanged && iface->id == characterinterface){
							//printf("agency changed\n");
							User * user = world.lobby.GetUserInfo(world.lobby.accountid);
							if(user && !user->retrieving){
								Uint8 selectedagency = GetSelectedAgency();
								switch(overlay->uid){
									case 2:{
										overlay->text = "LEVEL: " + std::to_string(user->agency[selectedagency].level);
									}break;
									case 3:{
										overlay->text = "WINS: " + std::to_string(user->agency[selectedagency].wins);
									}break;
									case 4:{
										overlay->text = "LOSSES: " + std::to_string(user->agency[selectedagency].losses);
									}break;
									case 5:{
										// xptonextlevel is accumulated XP toward current level;
										// threshold is 100*(level+1). Display the remaining amount.
										int lvl = user->agency[selectedagency].level;
										int remaining = 100 * (lvl + 1) - (int)user->agency[selectedagency].xptonextlevel;
										overlay->text = "XP TO NEXT LEVEL: " + std::to_string(remaining);
										agencychanged = false;
									}break;
								}
							}
						}
					}
				}break;
				case ObjectTypes::BUTTON:{
					Button * button = static_cast<Button *>(object);
					if(button && button->clicked && button->type != Button::BCHECKBOX){
						button->clicked = false;
						switch(button->uid){
							case 10:{ // go back
								if(GoBack()){
									return false;
								}
							}break;
							case 20:{ // join game
								if(gameselectinterface){
									Interface * gameselectiface = static_cast<Interface *>(world.GetObjectFromId(gameselectinterface));
									if(gameselectiface){
										for(std::vector<Uint16>::iterator it2 = gameselectiface->objects.begin(); it2 != gameselectiface->objects.end(); it2++){
											Object * object = world.GetObjectFromId(*it2);
											if(object && object->type == ObjectTypes::SELECTBOX){
												SelectBox * selectbox = static_cast<SelectBox *>(object);
												if(selectbox->selecteditem != -1){
													Uint32 gameid = selectbox->IndexToId(selectbox->selecteditem);
													if(gameid){
														LobbyGame * lobbygame = world.lobby.GetGameById(gameid);
														if(lobbygame){
															if(world.state == World::IDLE){
																User * user = world.lobby.GetUserInfo(world.lobby.accountid);
																bool canjoin = true;
																if(user){
																	if(lobbygame->minlevel > user->agency[GetSelectedAgency()].level){
																		canjoin = false;
																		CreateModalDialog("Your player level is too low");
																	}else
																	if(lobbygame->maxlevel < user->agency[GetSelectedAgency()].level){
																		canjoin = false;
																		CreateModalDialog("Your player level is too high");
																	}
																}
																if(canjoin){
																	currentlobbygameid = lobbygame->id;
																	if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
																		currentinterface = CreatePasswordDialog()->id;
																	}else{
																		JoinGame(*lobbygame);
																	}
																}
															}
														}
													}
												}else{
													CreateModalDialog("No game selected");
												}
											}
										}
									}
								}
							}break;
							case 25:{ // start game/ready
								if(gamejoininterface){
									Peer * localpeer = world.peerlist[world.localpeerid];
									bool ishost = localpeer && localpeer->ishost;
									if(!ishost || world.AllPeersDownloadedMap()){
										world.SendReady();
									}
								}
							}break;
							case 26:{ // change team
								if(gamejoininterface){
									world.ChangeTeam();
								}
							}break;
							case 27:{ // choose tech
								if(gamejoininterface){
									Object * object = world.GetObjectFromId(currentinterface);
									Interface * iface = static_cast<Interface *>(object);
									Interface * gamejoiniface = static_cast<Interface *>(world.GetObjectFromId(gamejoininterface));
									if(gamejoiniface){
										gamejoiniface->DestroyInterface(world, iface);
									}
									world.choosingtech = true;
									ShowTeamOverlays(false);
									world.RequestPeerList();
									gamejoininterface = 0;
									gametechinterface = CreateGameTechInterface()->id;
									iface->AddObject(gametechinterface);
									iface->activeobject = gametechinterface;
									Interface * chatiface = static_cast<Interface *>(world.GetObjectFromId(chatinterface));
									if(chatiface){
										chatiface->activeobject = 0;
									}
									currentinterface = iface->id;
									iface->ActiveChanged(world, iface, false);
									UpdateTechInterface();
									return false;
								}
							}break;
							case 28:{ // back to teams
								if(!gamejoininterface){
									Object * object = world.GetObjectFromId(currentinterface);
									Interface * iface = static_cast<Interface *>(object);
									Interface * gametechiface = static_cast<Interface *>(world.GetObjectFromId(gametechinterface));
									if(gametechiface){
										gametechiface->DestroyInterface(world, iface);
									}
									gametechinterface = 0;
									gamejoininterface = CreateGameJoinInterface()->id;
									iface->AddObject(gamejoininterface);
									world.choosingtech = false;
									ShowTeamOverlays(true);
									iface->activeobject = gamejoininterface;
									Interface * chatiface = static_cast<Interface *>(world.GetObjectFromId(chatinterface));
									if(chatiface){
										chatiface->activeobject = 0;
									}
									iface->ActiveChanged(world, iface, false);
									return false;
								}
							}break;
							case 30:{ // create game
								if(gameselectinterface){
									Object * object = world.GetObjectFromId(currentinterface);
									Interface * iface = static_cast<Interface *>(object);
									if(gameselectinterface){
										Interface * gameselectiface = static_cast<Interface *>(world.GetObjectFromId(gameselectinterface));
										if(gameselectiface){
											gameselectiface->DestroyInterface(world, iface);
										}
										gameselectinterface = 0;
									}
									gamecreateinterface = CreateGameCreateInterface()->id;
									iface->AddObject(gamecreateinterface);
									iface->activeobject = gamecreateinterface;
									Interface * chatiface = static_cast<Interface *>(world.GetObjectFromId(chatinterface));
									if(chatiface){
										chatiface->activeobject = 0;
									}
									iface->ActiveChanged(world, iface, false);
									return false;
								}
							}break;
							case 35:{ // create game create
								if(!creategameclicked){
									const char * gamename = "";
									const char * mapname = "";
									const char * password = 0;
									Interface * gamecreateiface = static_cast<Interface *>(world.GetObjectFromId(gamecreateinterface));
									if(gamecreateiface){
										Object * tobject = gamecreateiface->GetObjectWithUid(world, 5);
										if(tobject){
											TextInput * textinput = static_cast<TextInput *>(tobject);
											gamename = textinput->text;
										}
										tobject = gamecreateiface->GetObjectWithUid(world, 6);
										if(tobject){
											TextInput * textinput = static_cast<TextInput *>(tobject);
											if(strlen(textinput->text) > 0){
												password = textinput->text;
											}
										}
										Uint8 securitylevel = LobbyGame::SECNONE;
										tobject = gamecreateiface->GetObjectWithUid(world, 40);
										if(tobject){
											Button * button = static_cast<Button *>(tobject);
											if(strcmp(button->text, "Low") == 0){
												securitylevel = LobbyGame::SECLOW;
											}else
											if(strcmp(button->text, "Medium") == 0){
												securitylevel = LobbyGame::SECMEDIUM;
											}else
											if(strcmp(button->text, "High") == 0){
												securitylevel = LobbyGame::SECHIGH;
											}
										}
										Uint8 minlevel = 0;
										tobject = gamecreateiface->GetObjectWithUid(world, 41);
										if(tobject){
											TextInput * textinput = static_cast<TextInput *>(tobject);
											minlevel = atoi(textinput->text);
										}
										Uint8 maxlevel = 0;
										tobject = gamecreateiface->GetObjectWithUid(world, 42);
										if(tobject){
											TextInput * textinput = static_cast<TextInput *>(tobject);
											maxlevel = atoi(textinput->text);
										}
										Uint8 maxplayers = 0;
										tobject = gamecreateiface->GetObjectWithUid(world, 43);
										if(tobject){
											TextInput * textinput = static_cast<TextInput *>(tobject);
											maxplayers = atoi(textinput->text);
										}
										if(maxplayers <= 0){
											maxplayers = 1;
										}
										Uint8 maxteams = 0;
										tobject = gamecreateiface->GetObjectWithUid(world, 44);
										if(tobject){
											TextInput * textinput = static_cast<TextInput *>(tobject);
											maxteams = atoi(textinput->text);
										}
										if(maxteams <= 0){
											maxteams = 1;
										}
										if(strlen(gamename) == 0){
											CreateModalDialog("No game name");
										}else{
											tobject = gamecreateiface->GetObjectWithUid(world, 4);
											if(tobject){
												SelectBox * selectbox = static_cast<SelectBox *>(tobject);
												if(selectbox->selecteditem >= 0){
													mapname = selectbox->GetItemName(selectbox->selecteditem);
												if(mapDownloader.servermaps.count(mapname) > 0){
													CreateModalDialog("Download the map first");
												}else{
													unsigned char maphash[20];
													mapDownloader.CalculateMapHash(mapDownloader.FindMap(mapname).c_str(), &maphash);
													// Store args for deferred CreateGame after upload.
													mapDownloader.pendingCreate.gamename = gamename;
													mapDownloader.pendingCreate.mapname  = mapname;
													mapDownloader.pendingCreate.password = password ? password : "";
													memcpy(mapDownloader.pendingCreate.maphash, maphash, 20);
													mapDownloader.pendingCreate.securitylevel = securitylevel;
													mapDownloader.pendingCreate.minlevel      = minlevel;
													mapDownloader.pendingCreate.maxlevel      = maxlevel;
													mapDownloader.pendingCreate.maxplayers    = maxplayers;
													mapDownloader.pendingCreate.maxteams      = maxteams;
													// Upload map before creating so the dedicated server
													// can find it by filename.  Duplicate SHA-1 is a no-op.
													if(mapDownloader.mapUploadThread.joinable()) mapDownloader.mapUploadThread.detach();
													uint32_t gen = ++mapDownloader.mapUploadGeneration;
													std::string mppath = mapDownloader.FindMap(mapname);
													std::string dataDir = GetDataDir();
													bool isBundledMap = dataDir.empty() || mppath.substr(0, dataDir.size()) != dataDir;
													std::string apiURL = Config::GetInstance().mapapiurl;
													if(isBundledMap){
														// Bundled maps ship with the game; no upload needed.
														mapDownloader.mapUploadState.store(2, std::memory_order_release);
													}else{
														mapDownloader.mapUploadState.store(1, std::memory_order_relaxed);
														mapDownloader.mapUploadThread = std::thread([this, mapname_str=std::string(mapname), mppath, apiURL, gen](){
															bool ok = UploadMapToServer(mapname_str.c_str(), mppath.c_str(), apiURL.c_str());
															if(mapDownloader.mapUploadGeneration.load(std::memory_order_relaxed) != gen) return;
															mapDownloader.mapUploadState.store(ok ? 2 : 3, std::memory_order_release);
														});
													}
													creategameclicked = true;
													strcpy(Config::GetInstance().defaultgamename, gamename);
													Config::GetInstance().Save();
													CreateModalDialog("Uploading map...", false);
												}
												}else{
													CreateModalDialog("No map selected");
												}
											}
										}
									}
								}
							}break;
							case 40:{ // security level
								if(gamecreateinterface){
									Interface * gamecreateiface = static_cast<Interface *>(world.GetObjectFromId(gamecreateinterface));
									if(gamecreateiface){
										if(strcmp(button->text, "Off") == 0){
											strcpy(button->text, "Low");
										}else
										if(strcmp(button->text, "Low") == 0){
											strcpy(button->text, "Medium");
										}else
										if(strcmp(button->text, "Medium") == 0){
											strcpy(button->text, "High");
										}else
										if(strcmp(button->text, "High") == 0){
											strcpy(button->text, "Off");
										}
									}
								}
							}break;
						case 50: // modal ok button pressed
								if(modalinterface){
									Interface * modaliface = static_cast<Interface *>(world.GetObjectFromId(modalinterface));
									if(iface->id == passwordinterface && modaliface){
										TextInput * passwordinput = static_cast<TextInput *>(modaliface->GetObjectWithUid(world, 1));
										LobbyGame * lobbygame = world.lobby.GetGameById(currentlobbygameid);
										if(lobbygame && passwordinput){
											JoinGame(*lobbygame, passwordinput->text);
										}
									}
									DestroyModalDialog();
									creategameclicked = false;
									if(gamejoininterface || gametechinterface){
										if(GoBack()){
											return false;
										}
									}
									return false;
								}
							break;
						}
					}
				}break;
			}
		}
	}
	return true;
}

void Game::ProcessGameSummaryInterface(Interface * iface){
	if(world.lobby.statupgraded || !gamesummaryinfoloaded){
		User * user = world.lobby.GetUserInfo(world.lobby.accountid);
		if(user && !user->retrieving){
			UpdateGameSummaryInterface();
			world.lobby.statupgraded = false;
		}
	}
	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(object){
			switch(object->type){
				case ObjectTypes::TEXTBOX:{
					TextBox * textbox = static_cast<TextBox *>(object);
					if(textbox){
						Object * object = world.GetObjectFromId(iface->scrollbar);
						ScrollBar * scrollbar = static_cast<ScrollBar *>(object);
						if(scrollbar){
							textbox->scrolled = scrollbar->scrollposition;
						}
					}
				}break;
				case ObjectTypes::BUTTON:{
					Button * button = static_cast<Button *>(object);
					if(button && button->clicked){
						button->clicked = false;
						User * user = world.lobby.GetUserInfo(world.lobby.accountid);
						switch(button->uid){
							case 0:{ // continue
								if(world.lobby.state == Lobby::AUTHENTICATED){
									GoToState(LOBBY);
									world.lobby.JoinChannel(lastchannel);
								}else{
									GoToState(MAINMENU);
								}
							}break;
							case 10:{ // upgrade endurance
								world.lobby.UpgradeStat(user->statsagency, 0);
							}break;
							case 11:{ // upgrade shield
								world.lobby.UpgradeStat(user->statsagency, 1);
							}break;
							case 12:{ // upgrade jetpack
								world.lobby.UpgradeStat(user->statsagency, 2);
							}break;
							case 13:{ // upgrade techslots
								world.lobby.UpgradeStat(user->statsagency, 3);
							}break;
							case 14:{ // upgrade hacking
								world.lobby.UpgradeStat(user->statsagency, 4);
							}break;
							case 15:{ // upgrade contacts
								world.lobby.UpgradeStat(user->statsagency, 5);
							}break;
						}
					}
				}
			}
		}
	}
}

void Game::UpdateLobbyMapName(const char * name){
	Interface * lobbyiface = static_cast<Interface *>(world.GetObjectFromId(lobbyinterface));
	if(lobbyiface){
		for(std::vector<Uint16>::iterator it = lobbyiface->objects.begin(); it != lobbyiface->objects.end(); it++){
			Object * object = world.GetObjectFromId(*it);
			if(object->type == ObjectTypes::OVERLAY){
				Overlay * overlay = static_cast<Overlay *>(object);
				if(overlay->uid == 8){
					overlay->text = name;
					overlay->text = overlay->text.substr(0, 25);
				}
			}
		}
	}
}

void Game::UpdateTechInterface(void){
	if(gametechinterface){
		int techslotsleft = 0;
		Interface * gametechiface = static_cast<Interface *>(world.GetObjectFromId(gametechinterface));
		if(gametechiface){
			Overlay * overlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 70));
			if(overlay){
				Peer * peer = world.peerlist[world.localpeerid];
				if(peer){
					Team * team = world.GetPeerTeam(world.localpeerid);
					User * user = world.lobby.GetUserInfo(peer->accountid);
					if(user && team){
						techslotsleft = user->agency[team->agency].techslots - world.TechSlotsUsed(*peer);
						overlay->text = "Tech slots left: " + std::to_string(techslotsleft);
					}
				}else{
					if(world.tickcount % 12 == 0){
						// fix for when the tech interface doesnt appear if the peerlist packet was lost
						world.RequestPeerList();
					}
				}
			}
		}
		Team * team = world.GetPeerTeam(world.localpeerid);
		if(team){
			int peerindex = 0;
			for(int i = 0; i < 4; i++){
				Peer * peer = world.peerlist[team->peers[i]];
				User * user = 0;
				if(peer){
					user = world.lobby.GetUserInfo(peer->accountid);
				}
				bool draw = true;
				if(i >= team->numpeers){
					draw = false;
				}
				int b = 0;
				int bpos = 0;
				for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
					BuyableItem * buyableitem = *it;
					if(buyableitem->techslots){
						if(buyableitem->agencyspecific == -1 || buyableitem->agencyspecific == team->agency){
							bool usable = true;
							Uint8 uid = 110 + (30 * peerindex) + b;
							if(team->peers[i] == world.localpeerid){
								uid = 110 + (30 * 3) + b;
								if(buyableitem->techslots <= techslotsleft || (peer && peer->techchoices & buyableitem->techchoice)){
									usable = false;
								}
							}
							Interface * gametechiface = static_cast<Interface *>(world.GetObjectFromId(gametechinterface));
							if(gametechiface){
								Button * button = static_cast<Button *>(gametechiface->GetObjectWithUid(world, uid));
								if(button){
									if(peer && peer->techchoices & buyableitem->techchoice){
										button->res_index = 18; // on
									}else{
										button->res_index = 19; // off
									}
									if(team->peers[i] == world.localpeerid){
										if(!usable){
											button->effectbrightness = 128;
										}else{
											button->effectbrightness = 64;
										}
									}
									if(button && button->type == Button::BCHECKBOX){
										if(button->clicked){
											if(button->uid >= 200 && button->effectbrightness == 128){
												//Uint32 techchoice = 1 << (button->uid - 200);
												Peer * peer = world.peerlist[world.localpeerid];
												//printf("tech slots used: %d, %d\n", world.TechSlotsUsed(*peer), peer->techchoices);
												if(peer){
													world.SetTech(peer->techchoices ^ buyableitem->techchoice);
													Team * team = world.GetPeerTeam(world.localpeerid);
													if(team){
														Config::GetInstance().defaulttechchoices[team->agency] = peer->techchoices ^ buyableitem->techchoice;
														Config::GetInstance().Save();
													}
												}
											}
											button->clicked = false;
										}
									}
									button->draw = draw;
								}
							}
							if(gametechiface){
								Overlay * overlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 230 + b));
								if(overlay){
									if(overlay->clicked){
										overlay->clicked = false;
										Overlay * nameoverlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 60));
										if(nameoverlay){
											nameoverlay->text = "-" + std::string(buyableitem->name) + "-";
											nameoverlay->x = 401 + (116 - ((nameoverlay->text.length() * nameoverlay->textwidth) / 2));
										}
										char desc[1024];
										strcpy(desc, buyableitem->description);
										int linenum = 0;
										char * descline = strtok(desc, "\n");
										while(descline){
											Overlay * descoverlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 61 + linenum));
											if(descoverlay){
												descoverlay->text = descline;
											}
											linenum++;
											descline = strtok(NULL, "\n");
										}
										for(int i = linenum; i < 9; i++){
											Overlay * descoverlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 61 + i));
											if(descoverlay){
												descoverlay->text = "";
											}
										}
									}
									if(team->peers[i] == world.localpeerid){
										if(!usable){
											overlay->effectbrightness = 128;
										}else{
											overlay->effectbrightness = 64;
										}
									}
								}
							}
							bpos++;
						}
						b++;
					}
				}
				if(team->peers[i] != world.localpeerid){
					Interface * gametechiface = static_cast<Interface *>(world.GetObjectFromId(gametechinterface));
					if(gametechiface){
						Overlay * overlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 80 + peerindex));
						Overlay * overlayline = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 90 + peerindex));
						if(overlay && overlayline){
							overlay->draw = draw;
							if(user){
								overlay->text = user->name;
								overlay->x = 375 - (overlay->text.length() * overlay->textwidth);
							}
							overlayline->draw = draw;
						}
					}
					peerindex++;
				}
			}
		}
	}
}

void Game::UpdateGameSummaryInterface(void){
	if(!gamesummaryinterface){
		return;
	}
	Interface * gamesummaryiface = static_cast<Interface *>(world.GetObjectFromId(gamesummaryinterface));
	if(gamesummaryiface){
		//printf("Updated game summary\n");
		bool upgradeavailable = false;
		bool upgradesavailable[6];
		for(int i = 0; i < 6; i++){
			upgradesavailable[i] = false;
		}
		int totalbonusupgrades = 0;
		User * user = world.lobby.GetUserInfo(world.lobby.accountid);
		if(user && !user->retrieving){
			//printf("user found\n");
			gamesummaryinfoloaded = true;
			totalbonusupgrades += user->agency[user->statsagency].endurance;
			totalbonusupgrades += user->agency[user->statsagency].shield;
			totalbonusupgrades += user->agency[user->statsagency].jetpack;
			totalbonusupgrades += user->agency[user->statsagency].techslots;
			totalbonusupgrades += user->agency[user->statsagency].hacking;
			totalbonusupgrades += user->agency[user->statsagency].contacts;
			if(user->agency[user->statsagency].endurance < user->agency[user->statsagency].maxendurance){
				upgradesavailable[0] = true;
			}
			if(user->agency[user->statsagency].shield < user->agency[user->statsagency].maxshield){
				upgradesavailable[1] = true;
			}
			if(user->agency[user->statsagency].jetpack < user->agency[user->statsagency].maxjetpack){
				upgradesavailable[2] = true;
			}
			if(user->agency[user->statsagency].techslots < user->agency[user->statsagency].maxtechslots){
				upgradesavailable[3] = true;
			}
			if(user->agency[user->statsagency].hacking < user->agency[user->statsagency].maxhacking){
				upgradesavailable[4] = true;
			}
			if(user->agency[user->statsagency].contacts < user->agency[user->statsagency].maxcontacts){
				upgradesavailable[5] = true;
			}
			int maxupgrades = user->agency[user->statsagency].level;
			if(maxupgrades > user->TotalUpgradePointsPossible(user->statsagency)){
				maxupgrades = user->TotalUpgradePointsPossible(user->statsagency);
			}
			if(totalbonusupgrades - user->agency[user->statsagency].defaultbonuses < maxupgrades){
				upgradeavailable = true;
			}
		}
		for(int i = 0; i < 6; i++){
			if(upgradesavailable[i] && upgradeavailable){
				if(!gamesummaryiface->GetObjectWithUid(world, 10 + i)){
					Button * button = (Button *)world.CreateObject(ObjectTypes::BUTTON);
					button->y = -180 + (i * 46);
					button->x = 62;
					button->uid = 10 + i;
					switch(i){
						case 0: sprintf(button->text, "+1 Endurance"); break;
						case 1: sprintf(button->text, "+1 Shield   "); break;
						case 2: sprintf(button->text, "+1 Jetpack  "); break;
						case 3: sprintf(button->text, "+1 Tech Slot"); break;
						case 4: sprintf(button->text, "+1 Hacking  "); break;
						case 5: sprintf(button->text, "+1 Contacts "); break;
					}
					gamesummaryiface->AddObject(button->id);
				}
			}else{
				Button * button = static_cast<Button *>(gamesummaryiface->GetObjectWithUid(world, 10 + i));
				if(button){
					gamesummaryiface->RemoveObject(button->id);
					world.MarkDestroyObject(button->id);
				}
			}
			Overlay * overlay = static_cast<Overlay *>(gamesummaryiface->GetObjectWithUid(world, 20 + i));
			if(overlay){
				switch(i){
					case 0: overlay->text = std::to_string(user->agency[user->statsagency].endurance); break;
					case 1: overlay->text = std::to_string(user->agency[user->statsagency].shield); break;
					case 2: overlay->text = std::to_string(user->agency[user->statsagency].jetpack); break;
					case 3: overlay->text = std::to_string(user->agency[user->statsagency].techslots); break;
					case 4: overlay->text = std::to_string(user->agency[user->statsagency].hacking); break;
					case 5: overlay->text = std::to_string(user->agency[user->statsagency].contacts); break;
				}
				overlay->x = 556 - (overlay->text.length() * overlay->textwidth);
			}
		}
		Overlay * overlay = static_cast<Overlay *>(gamesummaryiface->GetObjectWithUid(world, 1));
		if(overlay){
			if(upgradeavailable){
				overlay->draw = true;
			}else{
				overlay->draw = false;
			}
		}
	}
}

void Game::AddSummaryLine(TextBox & textbox, const char * name, Uint32 value, bool percentage){
	char string[256];
	char valuetext[64];
	sprintf(valuetext, "%d%s", value, percentage ? "%" : " ");
	int maxchars = textbox.width / textbox.fontwidth;
	int used = strlen(name) + strlen(valuetext);
	strcpy(string, name);
	for(int i = 0; i < maxchars - used; i++){
		strcat(string, " ");
	}
	strcat(string, valuetext);
	textbox.AddLine(string);
}

void Game::ShowTeamOverlays(bool show){
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		Object * object = *it;
		if(object->type == ObjectTypes::TEAM){
			Team * team = static_cast<Team *>(object);
			team->ShowOverlays(world, show);
		}
	}
}

Uint8 Game::GetSelectedAgency(void){
	Interface * characteriface = static_cast<Interface *>(world.GetObjectFromId(characterinterface));
	for(std::vector<Uint16>::iterator it = characteriface->objects.begin(); it != characteriface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(object && object->type == ObjectTypes::TOGGLE){
			Toggle * toggle = static_cast<Toggle *>(object);
			if(toggle && toggle->selected){
				switch(toggle->uid){
					case 1:
						return Team::NOXIS;
					break;
					case 2:
						return Team::LAZARUS;
					break;
					case 3:
						return Team::CALIBER;
					break;
					case 4:
						return Team::STATIC;
					break;
					case 5:
						return Team::BLACKROSE;
					break;
				}
			}
		}
	}
	return 0;
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

const char * Game::GetActionKeyDisplayName(Action a){
	static thread_local char buf[32];
	const auto& ab = keymap.Get(a);
	for(const auto& b : ab.bindings){
		if(b.keys.empty()) continue;
		const auto& k = b.keys[0];
		if(k.device == BindingDevice::Keyboard){
			return KeyMap::GetKeyName((SDL_Scancode)k.code);
		}
	}
	std::strncpy(buf, "(unbound)", sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	return buf;
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
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					//iface->lastsym = ascii;
					if(!skip){
						iface->ProcessKeyPress(world, ascii);
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
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					iface->lastsym = event.key.scancode;
					if(!skip){
						iface->ProcessKeyPress(world, ascii);
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
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					if(event.button.button == SDL_BUTTON_LEFT){
						int w, h;
						SDL_GetWindowSize(window, &w, &h);
						iface->ProcessMousePress(world, true, (float(event.button.x) / w) * 640, (float(event.button.y) / h) * 480);
					}
				}
			}break;
			case SDL_EVENT_MOUSE_BUTTON_UP:{
				if(event.button.button == SDL_BUTTON_LEFT){
					Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
					if(iface){
						int w, h;
						SDL_GetWindowSize(window, &w, &h);
						iface->ProcessMousePress(world, false, (float(event.button.x) / w) * 640, (float(event.button.y) / h) * 480);
					}
				}
			}break;
			case SDL_EVENT_MOUSE_MOTION:{
				Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
				if(iface){
					int w, h;
					SDL_GetWindowSize(window, &w, &h);
					iface->ProcessMouseMove(world, (float(event.button.x) / w) * 640, (float(event.button.y) / h) * 480);
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

void Game::DrainControlQueue(){
	if(controlPort <= 0) return;
	auto cmds = controlserver.DrainImmediate();
	for(auto& c : cmds){
		if(c.phase == ControlCommand::MULTI_FRAME){
			ControlDispatch::EnqueueWait(*this, std::move(c));
		} else {
			ControlDispatch::HandleImmediate(*this, c);
		}
	}
	// TickWaits intentionally NOT called here — it runs after the sim while-loop
	// in Loop() so the very first tick after enqueue counts.
	// Dedicated server has no rendering and never calls PostFrameReplies(), so
	// any POST_RENDER op (e.g. screenshot) would block its handler thread
	// forever. Fail them at receive time.
	if(world.dedicatedserver.active){
		auto pr = controlserver.DrainPostRender();
		for(auto& c : pr){
			if(!c.reply) continue;
			ControlReply rpl;
			rpl.id = c.id;
			rpl.ok = false;
			rpl.code = "WRONG_STATE";
			rpl.error = "post-render ops not supported in dedicated server mode";
			c.reply->set_value(rpl);
		}
	}
}

void Game::PostFrameReplies(){
	if(controlPort <= 0) return;
	auto cmds = controlserver.DrainPostRender();
	for(auto& c : cmds){
		ControlDispatch::HandlePostRender(*this, c);
	}
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

void Game::TickActiveScreen(){
	if(screenStack.empty()) return;
	screenStack.back()->Tick(screenContext);
}