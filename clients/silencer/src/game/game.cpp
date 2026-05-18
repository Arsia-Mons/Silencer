#include "game.h"
#include "controldispatch.h"
#include "sdl3gpubackend.h"
#include "tuibackend.h"
#include <math.h>
#include "state.h"
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
#include "camera.h"
#include "detonator.h"
#include "objecttypes.h"
#include "client/ui/hud/InGameHud.h"
#include "client/ui/hud/InGameOverlays.h"
#include "client/ui/views/HudView.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#ifdef SILENCER_HAVE_LOBBY_UI
#include "lobby_screen.h"
#endif
#include "update_screen.h"
#include "mission_summary_screen.h"
#include <algorithm>
#include <stdio.h>
#include <vector>

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
               uiClayService(uiClayBackend),
               clientUi(uiClayService),
               inGameUiController(world),
               mapDownloader(world),
               ambienceMixer(world, renderer, mapDownloader, fade_i),
               screenContext(*this, world, renderer, world.lobby, keymap, updater, ambienceMixer, mapDownloader, window, renderdevice){
	world.SetVersion(SILENCER_VERSION);
	frames = 0;
	fps = 0;
	state = MAINMENU;
	stateisnew = true;
	fade_i = 0;
	fadeStartMs = 0;
	sharedstate = 0;
	currentlobbygameid = 0;
	lastannouncedgameid = 0;
	lastannouncedstatus = 0;
	joininggame = false;
	memset(keystate, 0, sizeof(keystate));
	gamepad = nullptr;
	singleplayermessage = 0;
	updatetitle = true;
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
	chatEnterDebounce = false;
	fullscreentoggled = false;
	replayfile = 0;
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
			window = SDL_CreateWindow("Silencer", screenbuffer.w, screenbuffer.h,
				SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
				(Config::GetInstance().fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
			SDL_StartTextInput(window);
			SyncRenderSurfaceToWindowPixels();
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
	if(!world.resources.Load(this, world.dedicatedserver.active)){
		printf("Could not load resources\n");
		return false;
	}
	// GAS is loaded inside resources.Load() — rebuild buyable items now that
	// GASLoader::Get().items is populated (World constructor ran too early).
	world.LoadBuyableItems();
	printf("Resources loaded\n");
	lasttick = SDL_GetTicks();
	RestartPaletteFade();
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

static const int kLegacyRenderWidth = 640;
static const int kLegacyRenderHeight = 480;

static float GameplayUiScaleForSurface(int width, int height) {
	int scaleX = width / kLegacyRenderWidth;
	int scaleY = height / kLegacyRenderHeight;
	int uiScale = scaleX < scaleY ? scaleX : scaleY;
	return static_cast<float>(uiScale > 0 ? uiScale : 1);
}

static float MenuUiScaleForSurface(int width, int height) {
	// Menus keep the legacy 640x480 design density as a lower bound, but the
	// scale changes continuously instead of snapping between integer steps.
	// That preserves readable bitmap text/chrome while avoiding the abrupt
	// "everything halves" jump as a desktop window crosses a 640px/480px
	// multiple.
	float scaleX = static_cast<float>(width) / static_cast<float>(kLegacyRenderWidth);
	float scaleY = static_cast<float>(height) / static_cast<float>(kLegacyRenderHeight);
	float uiScale = scaleX < scaleY ? scaleX : scaleY;
	return uiScale > 1.0f ? uiScale : 1.0f;
}

static void CenteredLayoutOffset(int surfaceW, int surfaceH,
                                 int virtualW, int virtualH,
                                 float scale, int& offsetX, int& offsetY) {
	int scaledW = static_cast<int>(virtualW * scale + 0.5f);
	int scaledH = static_cast<int>(virtualH * scale + 0.5f);
	offsetX = scaledW < surfaceW ? (surfaceW - scaledW) / 2 : 0;
	offsetY = scaledH < surfaceH ? (surfaceH - scaledH) / 2 : 0;
}

bool Game::ResizeRenderSurfacePixels(int width, int height){
	if(width < 1 || height < 1) return false;
	if(screenbuffer.w == width && screenbuffer.h == height) return true;
	screenbuffer.Resize(width, height, 0);
	return true;
}

bool Game::SyncRenderSurfaceToWindowPixels(){
	if(world.map.loaded){
		return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
	}
	if(!window) return false;
	int width = 0;
	int height = 0;
	if(!SDL_GetWindowSizeInPixels(window, &width, &height) || width < 1 || height < 1){
		SDL_GetWindowSize(window, &width, &height);
	}
	return ResizeRenderSurfacePixels(width, height);
}

bool Game::ResizeRenderSurface(int width, int height){
	if(width < 1 || height < 1) return false;
	if(window){
		SDL_SetWindowSize(window, width, height);
		if(world.map.loaded){
			return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
		}
		return SyncRenderSurfaceToWindowPixels();
	}
	if(world.map.loaded){
		return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
	}
	return ResizeRenderSurfacePixels(width, height);
}

bool Game::HasUiInputTarget() {
	if(GetTopScreen()) return true;
	return inGameUiController.HasInputTarget(world.localpeerid);
}

void Game::PrepareClientUiFrame(Surface& surface) {
	// UI magnification factor: gameplay keeps the strict legacy 640x480 fit,
	// while menus use a continuous scale so bitmap UI density changes smoothly
	// as the window resizes. Clay still lays out in virtual space; the
	// compositor scales the authored pixels back up into the native surface.
	float uiScale = world.map.loaded
		? GameplayUiScaleForSurface(surface.w, surface.h)
		: MenuUiScaleForSurface(surface.w, surface.h);
	int virtualW;
	int virtualH;
	if(world.map.loaded){
		// In-game: the whole frame is authored at the legacy 640x480 size.
		// The render backend stretches that final frame to the swapchain,
		// preserving origin/main's presentation behavior and frame cost.
		virtualW = kLegacyRenderWidth;
		virtualH = kLegacyRenderHeight;
	}else{
		// Menus reflow responsively — Clay lays out at the native surface
		// size divided by uiScale.
		virtualW = std::max(1, static_cast<int>(surface.w / uiScale));
		virtualH = std::max(1, static_cast<int>(surface.h / uiScale));
	}
	float mx = static_cast<float>(world.localinput.mousex);
	float my = static_cast<float>(world.localinput.mousey);
	bool down = world.localinput.mousedown;
	if(window){
		Uint32 buttons = SDL_GetMouseState(&mx, &my);
		down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
		int windowW = 0;
		int windowH = 0;
		SDL_GetWindowSize(window, &windowW, &windowH);
		float pixelX = mx;
		float pixelY = my;
		if(windowW > 0 && windowH > 0 && surface.w > 0 && surface.h > 0){
			pixelX = (mx / static_cast<float>(windowW)) * static_cast<float>(surface.w);
			pixelY = (my / static_cast<float>(windowH)) * static_cast<float>(surface.h);
		}
		int offsetX = 0;
		int offsetY = 0;
		CenteredLayoutOffset(surface.w, surface.h, virtualW, virtualH,
		                     uiScale, offsetX, offsetY);
		clientUiInput.SetPolledSurfacePointer(
			(pixelX - static_cast<float>(offsetX)) / static_cast<float>(uiScale),
			(pixelY - static_cast<float>(offsetY)) / static_cast<float>(uiScale),
			down);
	}else{
		clientUiInput.SetPolledSurfacePointer(mx, my, down);
	}
	float deltaTimeSeconds =
		static_cast<float>(GASLoader::Get().gameengine.tickIntervalMs) / 1000.0f;
	preparedUiInput = clientUiInput.BuildFrame(virtualW, virtualH, uiScale, deltaTimeSeconds);
	Uint64 now = SDL_GetTicks();
	float animationDeltaSeconds = 0.0f;
	if(lastUiAnimationMs != 0 && now >= lastUiAnimationMs){
		animationDeltaSeconds = static_cast<float>(now - lastUiAnimationMs) / 1000.0f;
		if(animationDeltaSeconds > 0.25f) animationDeltaSeconds = 0.25f;
	}
	lastUiAnimationMs = now;
	preparedUiInput.animationDeltaSeconds = animationDeltaSeconds;
	preparedUiInput.animationStepSeconds = LegacyUiAnimationStepSeconds();
	hasPreparedUiInput = true;
}

void Game::BeginPreparedClientUiFrame() {
	if(!hasPreparedUiInput) {
		PrepareClientUiFrame(screenbuffer);
	}
	silencer::clay_bridge::SetTextMeasureResources(&world.resources);
	clientUi.BeginFrame(preparedUiInput);
}

Clay_RenderCommandArray Game::EndClientUiFrame() {
	clientUi.EndFrame();
	hasPreparedUiInput = false;
	return uiClayBackend.Commands();
}

void Game::BuildVisibleClientUi(Surface& surface, float frametime) {
	clientUi.BuildVisibleScreens(screenContext, surface, frametime);
	if(world.map.loaded){
		// HUD owns Clay layout only; the system-camera insets + minimap are
		// world pixels drawn into the world surface by the render loop.
		// Build the HUD/overlay Clay declarations from the snapshot view.
		silencer::client_ui::HudView hudView =
			silencer::client_ui::BuildHudView(world);
		silencer::client_ui::BuildInGameHudUi(
			renderer, world.resources, hudView, &surface, clientUi.Interactions());
		silencer::client_ui::BuildInGameOverlaysUi(renderer, world.resources, hudView, &surface);
	}
}

void Game::DrawInGameWorldInsets(Surface& surface, float frametime) {
	Player * localplayer = world.GetPeerPlayer(world.GetLocalPeerId());
	if(!localplayer) return;
	Renderer::Rect dstrect;
	for(int slot = 0; slot < 2; ++slot){
		if(!world.IsSystemCameraActive(slot)) continue;
		Surface systemscreen(135, 44, 1);
		Camera camera(135 * 2, 44 * 2);
		Object * followobject = world.GetObjectFromId(world.GetSystemCameraFollowId(slot));
		int px = 0;
		int py = 0;
		if(followobject){
			px = followobject->x + ((followobject->oldx - followobject->x) * frametime);
			py = followobject->y + ((followobject->oldy - followobject->y) * frametime);
			if(slot == 1 && followobject->type == ObjectTypes::DETONATOR){
				Detonator * detonator = static_cast<Detonator*>(followobject);
				if(detonator->HasDetonated() && py < detonator->lowestypos){
					py = detonator->lowestypos;
				}
			}
		}
		camera.Follow(world,
		              px + world.GetSystemCameraX(slot),
		              py + world.GetSystemCameraY(slot),
		              0, 0, 0, 0);
		renderer.DrawWorldScaled(&systemscreen, camera, 3, frametime);
		renderer.EffectRampColor(&systemscreen, 0, 190);
		dstrect.x = (slot == 0) ? 5 : 500;
		dstrect.y = (slot == 0) ? 349 : 348;
		Renderer::BlitSurface(&systemscreen, 0, &surface, &dstrect);
	}
	dstrect.x = 235;
	dstrect.y = 419;
	Renderer::BlitSurface(&world.map.minimap.surface, 0, &surface, &dstrect);
}

void Game::RenderClientUiFrame(Surface& surface, float frametime) {
	if(!clientUi.HasScreens() && !world.map.loaded){
		return;
	}

	PrepareClientUiFrame(surface);
	BeginPreparedClientUiFrame();
	BuildVisibleClientUi(surface, frametime);
	Clay_RenderCommandArray cmds = EndClientUiFrame();
	silencer::clay_bridge::Render(*this, &surface, cmds);
	if(state != FADEOUT){
		std::vector<silencer::ui::UiAction> unhandledUiActions =
			clientUi.DispatchInput(screenContext, preparedUiInput);
		if(!clientUi.HasScreens() && world.map.loaded){
			inGameUiController.ApplyActions(
				world.localpeerid, unhandledUiActions, clientUi.Interactions());
		}
	}
}

void Game::ResetUiFrameDeltas() {
	clientUiInput.EndFrame();
	preparedUiInput.pointer.wheelX = 0.0f;
	preparedUiInput.pointer.wheelY = 0.0f;
	preparedUiInput.textInput.clear();
	preparedUiInput.navActions.clear();
	preparedUiInput.bindingInputs.clear();
	preparedUiInput.controlCommands.clear();
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
		int width = std::min(500, screenbuffer.w - 32);
		int widthp = (float(progress) / totalprogressitems) * width;
		int barx = (screenbuffer.w - width) / 2;
		int bary = (screenbuffer.h - 32) / 2;
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

void Game::RestartPaletteFade(){
	fadeStartMs = SDL_GetTicks();
	fade_i = 0;
}

float Game::LegacyUiAnimationStepSeconds() const {
	const int hz = GASLoader::Get().gameengine.ticksPerSecond > 0
		? GASLoader::Get().gameengine.ticksPerSecond
		: 24;
	return 1.0f / static_cast<float>(hz);
}

Uint8 Game::PaletteFadePhaseFromClock() const {
	if(fadeStartMs == 0) return fade_i;
	Uint64 now = SDL_GetTicks();
	float elapsedSeconds = 0.0f;
	if(now >= fadeStartMs){
		elapsedSeconds = static_cast<float>(now - fadeStartMs) / 1000.0f;
	}
	int phase = static_cast<int>(elapsedSeconds / LegacyUiAnimationStepSeconds());
	if(phase < 0) phase = 0;
	if(phase > 16) phase = 16;
	return static_cast<Uint8>(phase);
}

bool Game::PaletteFadeFinished() const {
	return PaletteFadePhaseFromClock() >= 16;
}

void Game::ApplyPaletteFade(bool fadeOut){
	fade_i = PaletteFadePhaseFromClock();
	int phase = fade_i;
	if(phase > 15) phase = 15;
	if(fadeOut){
		SDL_Color * fadedpalette =
			renderer.palette.CopyWithBrightness(renderer.palette.GetColors(), (15 - phase) * 8);
		SetColors(fadedpalette);
		return;
	}
	if(phase >= 15){
		SetColors(renderer.palette.GetColors());
		return;
	}
	SDL_Color * fadedpalette =
		renderer.palette.CopyWithBrightness(renderer.palette.GetColors(), phase * 8);
	SetColors(fadedpalette);
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
		// Edge events (menu nav, text input) arrive as normalized per-frame
		// UI input and are dispatched by ClientUi after layout.
		if(tui){
			Uint8 newkeystate[SDL_SCANCODE_COUNT];
			if(inputserver.LatestScancodes(newkeystate)){
				std::vector<int> pressedScancodes;
				// Edge-detect: feed press/release transitions through the
				// same handlers the SDL path uses, so the in-game ESC
				// quitstate machine, F1 player-list, debug overlay etc.
				// behave identically with a TUI keyboard.
				for(int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc){
					bool was = keystate[sc] != 0;
					bool now = newkeystate[sc] != 0;
					if(was == now) continue;
					if(now){
						OnScancodeDown(sc);
						pressedScancodes.push_back(sc);
					}else{
						OnScancodeUp(sc);
					}
				}
				memcpy(keystate, newkeystate, sizeof(keystate));
				for(int sc : pressedScancodes){
					QueueUiKeyboardInputForScancode(sc);
				}
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
			}
		} else {
			UpdateInputState(world.localinput);
			clientUiInput.CaptureGamepadBindingEdges(
				gamepadstate.buttons, gamepadstate.axes,
				SDL_GAMEPAD_AXIS_COUNT, AXIS_DEADZONE);
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
		lasttick += wait;
	}
	// Tick multi-frame waits AFTER the sim loop so wait_frames --n 1 and
	// step --frames 1 see at least one sim tick before resolving. Putting this
	// inside DrainControlQueue (which runs before the sim loop) made the
	// counter race the enqueue and resolve with zero ticks.
	ControlDispatch::TickWaits(*this);
	world.DoNetwork();
	if(!world.dedicatedserver.active){
		world.DoNetwork();
		float ft = 1 - (float(tickcheck - lasttick) / wait);
		if(world.map.loaded){
			// origin/main rendered one 640x480 paletted frame and let the GPU
			// present pass stretch it to the window. Keep that path for
			// gameplay; native-sized CPU frames are too expensive fullscreen.
			ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
			screenbuffer.Clear(0);
			renderer.Draw(&screenbuffer, ft);
			DrawInGameWorldInsets(screenbuffer, ft);
		}else{
			if(window) SyncRenderSurfaceToWindowPixels();
			screenbuffer.Clear(0);
			renderer.Draw(&screenbuffer, ft);
		}
		RenderClientUiFrame(screenbuffer, ft);
#ifdef POSIX
		if(world.replay.IsPlaying() && world.replay.ffmpeg && world.replay.ffmpegvideo && deploymessageshown){
			std::vector<Uint8> buffer(screenbuffer.w * screenbuffer.h * 3);
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
			fwrite(buffer.data(), buffer.size(), 1, world.replay.ffmpeg);
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
		ResetUiFrameDeltas();
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
	clientUi.ClearScreensIfRequested(screenContext);
	if(state != FADEOUT){
		clientUi.TickVisibleScreens(screenContext);
	}
	inGameUiController.UpdateOverlayState(world.localpeerid);
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
			// Ready-button text refresh ("Waiting..." vs "Ready") happens
			// in GameJoinPanelTick — runs each frame from LobbyScreen::Tick.
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
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				// Button-click handling lives in MainMenuScreen::Tick, dispatched
				// by ClientUi's navigation stack at the top of Game::Tick.
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
#ifdef SILENCER_HAVE_LOBBY_UI
				PushScreen(std::make_unique<LobbyScreen>());
#endif
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
				// Lobby pump (state-machine + deferred-create) lives in
				// LobbyScreen::Tick, dispatched by ClientUi's navigation stack
				// at the top of Game::Tick.
			}
		}break;
		case UPDATING:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				PushScreen(std::make_unique<UpdateScreen>());
				stateisnew = false;
			}else{
				if(ambienceMixer.FadedIn()){
					ambienceMixer.PlayMusic(world.resources.menumusic);
				}
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
		case HOSTGAME: TickHostGame(); break;
		case JOINGAME: TickJoinGame(); break;
		case TESTGAME: TickTestGame(); break;
		case REPLAYGAME: TickReplayGame(); break;
	}
	if(fade_i < 16 && state != FADEOUT){
		ApplyPaletteFade(false);
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
	RestartPaletteFade();
	stateisnew = true;
	nextstateprocessed = false;
	// Keep the outgoing Clay screen mounted until TickFadeOut reaches black.
	// Legacy retained its world UI objects across FADEOUT, so there were still
	// pixels for the palette fade to dim before the next state rebuilt UI.
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
	if(!gamepadstate.connected) return;
	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	bool inGameUi = localplayer && (localplayer->chatActive || localplayer->isbuying || localplayer->techstationactive);
	Screen * top = GetTopScreen();
	if(!top && !inGameUi) return;

	Uint32 now = SDL_GetTicks();

	// Helper: fire a nav key press with software repeat on held direction.
	auto tick = [&](GamepadNavDir& dir, Action action, silencer::ui::UiNavAction navAction){
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
			clientUiInput.QueueNavAction(navAction);
		} else if(now >= dir.nextfire){
			// Repeat.
			dir.nextfire = now + GAMEPAD_NAV_REPEAT_MS;
			clientUiInput.QueueNavAction(navAction);
		}
	};

	tick(gamepadNavUp,    Action::UiUp,    silencer::ui::UiNavAction::Up);
	tick(gamepadNavDown,  Action::UiDown,  silencer::ui::UiNavAction::Down);
	tick(gamepadNavLeft,  Action::UiLeft,  silencer::ui::UiNavAction::Left);
	tick(gamepadNavRight, Action::UiRight, silencer::ui::UiNavAction::Right);

	// Confirm (A/Cross) is edge-triggered; directional nav handles repeat.
	{
		bool confirmNow = keymap.IsPressed(Action::UiConfirm, keystate, gamepadstate);
		static bool confirmPrev = false;
		if(confirmNow && !confirmPrev){
			clientUiInput.QueueNavAction(silencer::ui::UiNavAction::Confirm);
		}
		confirmPrev = confirmNow;
	}

	{
		bool cancelNow = keymap.IsPressed(Action::UiCancel, keystate, gamepadstate);
		static bool cancelPrev = false;
		if(cancelNow && !cancelPrev){
			clientUiInput.QueueNavAction(silencer::ui::UiNavAction::Cancel);
		}
		cancelPrev = cancelNow;
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

Game::WorldSummary Game::GetWorldSummary(){
	WorldSummary summary;
	summary.map = world.gameinfo.mapname;
	summary.peers = static_cast<int>(world.peercount);
	summary.localPeerId = static_cast<int>(world.localpeerid);
	summary.viewedPeerId = static_cast<int>(world.viewedpeerid);
	summary.authorityPeer = static_cast<int>(world.authoritypeer);
	summary.lobbyAccountId = static_cast<unsigned int>(world.lobby.accountid);
	summary.isLocalObserver = world.IsLocalObserver();
	summary.spectatorInitialized = world.spectator.initialized;
	summary.spectatorFreecam = world.spectator.freecam;
	summary.messageText = world.GetMessageText();
	summary.messageProgress = static_cast<int>(world.GetMessageProgress());
	summary.messageType = static_cast<int>(world.GetMessageType());
	summary.messageTime = static_cast<int>(world.GetMessageTime());
	summary.topMessageText = world.GetTopMessageText();
	summary.topMessageProgress = static_cast<int>(world.GetTopMessageProgress());
	for(unsigned int i = 0; i < world.maxpeers; i++){
		Peer * p = world.peerlist[i];
		if(!p) continue;
		WorldPeerSummary peer;
		peer.id = static_cast<int>(i);
		peer.accountId = static_cast<unsigned int>(p->accountid);
		peer.observer = p->observer;
		peer.disconnected = p->disconnected;
		for(Uint16 cid : p->controlledlist){
			peer.controlledList.push_back(static_cast<int>(cid));
		}
		summary.peerList.push_back(std::move(peer));
	}
	for(auto* o : world.objectlist){
		++summary.objectsCount;
		if(o && o->type == ObjectTypes::PLAYER){
			Player* p = (Player*)o;
			WorldPlayerSummary player;
			player.id = static_cast<int>(p->id);
			player.hp = static_cast<int>(p->health);
			player.x = static_cast<int>(p->x);
			player.y = static_cast<int>(p->y);
			summary.players.push_back(player);
		}
	}
	return summary;
}

bool Game::IsLiveMultiplayer() const {
	return (world.peercount > 1) && (world.gameplaystate == World::INGAME);
}

void Game::PushScreen(std::unique_ptr<Screen> s){
	clientUi.PushScreen(std::move(s), screenContext);
}

void Game::PopScreen(){
	clientUi.PopScreen(screenContext);
}

void Game::ReplaceScreen(std::unique_ptr<Screen> s){
	clientUi.ReplaceScreen(std::move(s), screenContext);
}

Screen * Game::GetTopScreen() const {
	return clientUi.TopScreen();
}
