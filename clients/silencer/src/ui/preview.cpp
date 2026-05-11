// Preview-mode harness for the ui/v2 declarative library.
//
// Implements `Game::RunPreview()` (declared in game.h). Dispatched from
// main.cpp when the binary is launched with --preview-screen=NAME.
//
// Two output modes:
//   * --dump-ppm=PATH : render once, write a P6 PPM, exit.
//                       Pair with --headless to skip window creation.
//   * (no dump path)  : open the SDL window and re-render every frame
//                       so saving a screen source rebuilds on next run.
//                       Mouse motion drives hover styling; left-click
//                       fires the Button on_click handlers wired below.

#include "game.h"

#include "context.h"

#include "layout.h"
#include "node.h"
#include "render.h"
#include "render_commands.h"
#include "ui_state.h"
#include "main_menu.h"
#include "options.h"
#include "options_display.h"
#include "options_audio.h"
#include "options_controls.h"
#include "lobby_connect.h"
#include "lobby_chat.h"
#include "lobby_shell.h"
#include "mission_summary.h"
#include "update.h"
#include "lobby_create.h"
#include "lobby_join.h"
#include "lobby_select.h"
#include "lobby_tech.h"
#include "modals/message.h"
#include "modals/password.h"

#include "map_downloader.h"
#include "mapfetch.h"
#include "os.h"
#include "config.h"
#include "renderer.h"
#include "renderdevice.h"
#include "resources.h"
#include "screen_context.h"
#include "surface.h"
#include "world.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef SILENCER_VERSION
#define SILENCER_VERSION "00000"
#endif

static bool DumpPPM(const Surface & buf, const SDL_Color * palette, const char * path)
{
	FILE * f = fopen(path, "wb");
	if(!f){
		fprintf(stderr, "[preview] fopen failed for '%s'\n", path);
		return false;
	}
	fprintf(f, "P6\n%d %d\n255\n", buf.w, buf.h);
	const Uint8 * pix = buf.pixels.data();
	const int n = buf.w * buf.h;
	// Tiny stack buffer keeps fwrite calls amortised; full 640x480 frame
	// is 921600 bytes — chunking avoids per-pixel fwrite syscalls.
	unsigned char chunk[1024 * 3];
	int written = 0;
	while(written < n){
		int batch = (n - written) < 1024 ? (n - written) : 1024;
		for(int i = 0; i < batch; i++){
			Uint8 idx = pix[written + i];
			chunk[i * 3 + 0] = palette[idx].r;
			chunk[i * 3 + 1] = palette[idx].g;
			chunk[i * 3 + 2] = palette[idx].b;
		}
		fwrite(chunk, 1, batch * 3, f);
		written += batch;
	}
	fclose(f);
	return true;
}

int Game::RunPreview()
{
	// Storybook is a self-contained interactive playground — bail out
	// early so we don't have to weave its UI through the per-screen
	// dispatch below. Lives in src/ui/storybook.cpp.
	if(strcmp(preview_screen, "storybook") == 0){
		return RunStorybook();
	}
	// Pick the palette the legacy screen would have loaded via
	// ResetPresentation. lobby_connect uses palette 2; everything else
	// migrated so far uses palette 1.
	const int palette_idx = (strcmp(preview_screen, "lobby_connect") == 0 ||
	                          strcmp(preview_screen, "lobby") == 0 ||
	                          strcmp(preview_screen, "lobby_create") == 0 ||
	                          strcmp(preview_screen, "lobby_join") == 0 ||
	                          strcmp(preview_screen, "lobby_select") == 0 ||
	                          strcmp(preview_screen, "lobby_tech") == 0 ||
	                          strcmp(preview_screen, "update") == 0) ? 2 : 1;
	if(!renderer.palette.SetPalette(palette_idx)){
		fprintf(stderr, "[preview] palette %d load failed\n", palette_idx);
		return 4;
	}
	SetColors(renderer.palette.GetColors());

	// Mouse position in logical 640×480 pixels. -1 = "no mouse this frame"
	// — Context defaults to this value; hover and click hit-tests bail
	// when mouse_x < 0, so the PPM dump path renders without any hover state.
	int mouse_x = -1;
	int mouse_y = -1;

	// Persistent UI state across frames (hot_t per button, etc.). Lives on
	// the stack here; will move into the engine UI subsystem when v2
	// replaces MainMenuScreen. Stays NULL inside the dump-PPM path so the
	// snapshot matches legacy byte-for-byte (no animation slots advance).
	ui::v2::UIState ui_state;

	float  frame_dt   = 0.0f;
	Uint64 last_ticks = 0;

	bool running = true;

	// Click handlers wired into the v2 MainMenu. Live game wiring (state
	// transitions on Tutorial / Connect / Options) lands when v2 replaces
	// MainMenuScreen in the engine; for the preview we just log the click
	// so authors can verify hit-testing without running the full game.
	ui::v2::MainMenuHandlers handlers;
	handlers.on_tutorial = [](){ printf("[preview] Tutorial clicked\n"); };
	handlers.on_lobby    = [](){ printf("[preview] Connect To Lobby clicked\n"); };
	handlers.on_options  = [](){ printf("[preview] Options clicked\n"); };
	handlers.on_exit     = [&running](){
		printf("[preview] Exit clicked\n");
		running = false;
	};

	ui::v2::OptionsHandlers options_handlers;
	options_handlers.on_controls = [](){ printf("[preview] Controls clicked\n"); };
	options_handlers.on_display  = [](){ printf("[preview] Display clicked\n"); };
	options_handlers.on_audio    = [](){ printf("[preview] Audio clicked\n"); };
	options_handlers.on_go_back  = [&running](){
		printf("[preview] Go Back clicked\n");
		running = false;
	};

	ui::v2::OptionsDisplayHandlers options_display_handlers;
	options_display_handlers.on_toggle_fullscreen     = [](){ printf("[preview] Fullscreen clicked\n"); };
	options_display_handlers.on_toggle_smooth_scaling = [](){ printf("[preview] Smooth Scaling clicked\n"); };
	options_display_handlers.on_save                  = [](){ printf("[preview] Save clicked\n"); };
	options_display_handlers.on_cancel                = [&running](){
		printf("[preview] Cancel clicked\n");
		running = false;
	};

	ui::v2::OptionsAudioHandlers options_audio_handlers;
	options_audio_handlers.on_toggle_music = [](){ printf("[preview] Music clicked\n"); };
	options_audio_handlers.on_save         = [](){ printf("[preview] Save clicked\n"); };
	options_audio_handlers.on_cancel       = [&running](){
		printf("[preview] Cancel clicked\n");
		running = false;
	};

	ui::v2::OptionsControlsHandlers options_controls_handlers;
	options_controls_handlers.on_preset = [](){ printf("[preview] Preset clicked\n"); };
	options_controls_handlers.on_save   = [](){ printf("[preview] Save clicked\n"); };
	options_controls_handlers.on_cancel = [&running](){
		printf("[preview] Cancel clicked\n");
		running = false;
	};

	ui::v2::LobbyConnectHandlers lobby_connect_handlers;
	lobby_connect_handlers.on_login  = [](){ printf("[preview] Login clicked\n"); };
	lobby_connect_handlers.on_cancel = [&running](){
		printf("[preview] Cancel clicked\n");
		running = false;
	};

	ui::v2::LobbyHandlers lobby_handlers;
	lobby_handlers.on_go_back = [&running](){
		printf("[preview] Go Back clicked\n");
		running = false;
	};
	lobby_handlers.game_create.on_security_toggle = [](){ printf("[preview] Security clicked\n"); };
	lobby_handlers.game_create.on_create          = [](){ printf("[preview] Create clicked\n"); };
	lobby_handlers.game_join.on_ready       = [](){ printf("[preview] Ready clicked\n"); };
	lobby_handlers.game_join.on_change_team = [](){ printf("[preview] Change Team clicked\n"); };
	lobby_handlers.game_join.on_choose_tech = [](){ printf("[preview] Choose Tech clicked\n"); };
	lobby_handlers.game_select.on_create = [](){ printf("[preview] Create Game clicked\n"); };
	lobby_handlers.game_select.on_join   = [](){ printf("[preview] Join Game clicked\n"); };
	lobby_handlers.game_tech.on_back_to_teams = [](){ printf("[preview] Back To Teams clicked\n"); };

	// Helper that mirrors GameCreatePanel::Build's legacy map-list
	// computation (CDResDir + ListFiles, then CDDataDir + ListFiles, then
	// FetchServerMapList for "[DL] " entries) so the v2 BuildLobby
	// receives the same items the legacy SelectBox would render. Lives
	// outside the render closure because both the dump-PPM and click
	// dispatch paths need the same state.
	auto compute_game_create_state = [&]() -> ui::v2::GameCreateState {
		ui::v2::GameCreateState gcs;
		gcs.game_name = Config::GetInstance().defaultgamename;
		std::vector<std::string> maps;
		CDResDir();
		auto local = screenContext.mapDownloader.ListFiles((GetResDir() + "level").c_str());
		maps.insert(maps.end(), local.begin(), local.end());
		CDDataDir();
		auto downloads = screenContext.mapDownloader.ListFiles((GetDataDir() + "level/download").c_str());
		for(auto & d : downloads){
			if(std::find(maps.begin(), maps.end(), d) == maps.end()) maps.push_back(d);
		}
		std::sort(maps.begin(), maps.end());
		auto serverlist = FetchServerMapList(Config::GetInstance().mapapiurl);
		for(auto & entry : serverlist){
			if(std::find(maps.begin(), maps.end(), entry.first) == maps.end()){
				std::string label = "[DL] " + entry.first;
				maps.push_back(label);
				gcs.server_maps.insert(label);
			}
		}
		gcs.map_items = std::move(maps);
		return gcs;
	};

	ui::v2::MissionSummaryHandlers mission_summary_handlers;
	mission_summary_handlers.on_done = [&running](){
		printf("[preview] Done clicked\n");
		running = false;
	};

	ui::v2::UpdateHandlers update_handlers;
	update_handlers.on_update   = [](){ printf("[preview] Update clicked\n"); };
	update_handlers.on_retry    = [](){ printf("[preview] Retry clicked\n"); };
	update_handlers.on_download = [](){ printf("[preview] Download clicked\n"); };
	update_handlers.on_cancel   = [&running](){
		printf("[preview] Cancel clicked\n");
		running = false;
	};

	const std::string message_modal_text = "Test message";
	ui::v2::MessageHandlers message_handlers;
	message_handlers.on_ok = [&running](){
		printf("[preview] OK clicked\n");
		running = false;
	};

	// Password modal preview state: empty text matches the post-Build,
	// pre-user-input state the legacy modal renders at preview-gate time.
	std::string password_modal_text;
	ui::v2::PasswordHandlers password_handlers;
	password_handlers.on_submit = [&running](const std::string & captured){
		printf("[preview] Password submitted: '%s'\n", captured.c_str());
		running = false;
	};

	// `with_state=false` is the dump-PPM path: NULL UIState → render
	// snaps + dt is ignored, so output stays byte-identical to legacy.
	auto make_ctx = [&](bool with_state){
		ui::v2::Context ctx{
			world.resources,
			/*logical_w=*/640,
			/*logical_h=*/480,
			/*scale=*/preview_scale,
			/*version=*/SILENCER_VERSION,
		};
		ctx.mouse_x = mouse_x;
		ctx.mouse_y = mouse_y;
		if(with_state){
			ctx.state = &ui_state;
			ctx.dt    = frame_dt;
		}
		return ctx;
	};

	// `with_state` controls whether the UIState path is active. The dump-PPM
	// caller passes false; the interactive loop passes true so animations
	// advance and stale IDs get GC'd at end of frame.
	auto render_once = [&](bool with_state){
		// Match the live engine's Path B sizing so visual scale-N captures
		// work — preview_scale=1 is the headless 640×480 gate; >1 grows the
		// framebuffer to hold scale-multiplied sprite blits.
		const int want_w = 640 * preview_scale;
		const int want_h = 480 * preview_scale;
		if(screenbuffer.w != want_w || screenbuffer.h != want_h){
			screenbuffer.Resize(want_w, want_h);
		}
		screenbuffer.Clear(0);
		{
			ui::v2::Context ctx = make_ctx(with_state);
			if(strcmp(preview_screen, "main_menu") == 0){
				ui::v2::EnsureClayContext(ctx);
				Clay_SetPointerState(Clay_Vector2{ (float)ctx.mouse_x, (float)ctx.mouse_y }, false);
				Clay_UpdateScrollContainers(false, Clay_Vector2{ 0.0f, 0.0f }, ctx.dt);
				Clay_SetLayoutDimensions(Clay_Dimensions{ (float)ctx.logical_w, (float)ctx.logical_h });
				Clay_BeginLayout();
				ui::v2::RenderMainMenu(ctx, handlers);
				Clay_RenderCommandArray cmds = Clay_EndLayout();
				ui::DrawRenderCommands(cmds, renderer, screenbuffer, ctx.scale);
			}else if(strcmp(preview_screen, "options") == 0){
				ui::v2::EnsureClayContext(ctx);
				Clay_SetPointerState(Clay_Vector2{ (float)ctx.mouse_x, (float)ctx.mouse_y }, false);
				Clay_UpdateScrollContainers(false, Clay_Vector2{ 0.0f, 0.0f }, ctx.dt);
				Clay_SetLayoutDimensions(Clay_Dimensions{ (float)ctx.logical_w, (float)ctx.logical_h });
				Clay_BeginLayout();
				ui::v2::RenderOptions(ctx, options_handlers);
				Clay_RenderCommandArray cmds = Clay_EndLayout();
				ui::DrawRenderCommands(cmds, renderer, screenbuffer, ctx.scale);
			}else if(strcmp(preview_screen, "options_display") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildOptionsDisplay(ctx, options_display_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "options_audio") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildOptionsAudio(ctx, options_audio_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "options_controls") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildOptionsControls(ctx, options_controls_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "lobby_connect") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildLobbyConnect(ctx, lobby_connect_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "lobby") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::LobbyState lobby_state;
					lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
					ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "lobby_create") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::LobbyState lobby_state;
				lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
				lobby_state.active_panel = ui::v2::LobbyActivePanel::GameCreate;
				lobby_state.game_create = compute_game_create_state();
				ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "lobby_join") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::LobbyState lobby_state;
				lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
				lobby_state.active_panel = ui::v2::LobbyActivePanel::GameJoin;
				ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "lobby_select") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::LobbyState lobby_state;
				lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
				lobby_state.active_panel = ui::v2::LobbyActivePanel::GameSelect;
				ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "lobby_tech") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::LobbyState lobby_state;
				lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
				lobby_state.active_panel = ui::v2::LobbyActivePanel::GameTech;
				ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "mission_summary") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildMissionSummary(ctx, mission_summary_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "message_modal") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildMessage(ctx, message_modal_text, /*has_ok=*/true, message_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "password_modal") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildPassword(ctx, password_modal_text, password_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "update") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildUpdate(ctx, update_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else{
				fprintf(stderr, "[preview] unknown screen '%s' for v2 impl\n", preview_screen);
			}
		}
	};

	render_once(/*with_state=*/false);

	if(dump_ppm_path[0]){
		if(!DumpPPM(screenbuffer, renderer.palette.GetColors(), dump_ppm_path)){
			return 3;
		}
		printf("[preview] screen=%s, scale=%d -> %s\n",
		       preview_screen, preview_scale, dump_ppm_path);
		return 0;
	}

	if(!window || !renderdevice){
		fprintf(stderr, "[preview] interactive mode needs a window — drop --headless or pass --dump-ppm\n");
		return 5;
	}

	// Interactive loop: re-render every frame so editing main_menu.cpp
	// and rebuilding (`cmake --build --preset win-ninja-unity`) reloads
	// the screen on the next launch. Hot reload without restart would
	// require a shared-lib swap; deferred — restart-on-rebuild is fine
	// for now (build is ~15s on Windows unity preset).
	printf("[preview] interactive %s, scale=%d  (Esc/close to exit)\n",
	       preview_screen, preview_scale);

	// Window→logical scaling matches game/events.cpp's mouse handlers:
	// SDL3GPUBackend upscales the 640×480 framebuffer to the (resizable)
	// window, so we divide by current window size and multiply by 640/480.
	auto window_to_logical = [&](float wx, float wy, int & out_x, int & out_y){
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		if(w <= 0 || h <= 0){ out_x = -1; out_y = -1; return; }
		out_x = (int)((wx / (float)w) * 640.0f);
		out_y = (int)((wy / (float)h) * 480.0f);
	};

	while(running){
		SDL_Event ev;
		while(SDL_PollEvent(&ev)){
			if(ev.type == SDL_EVENT_QUIT){
				running = false;
			}else if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE){
				running = false;
			}else if(ev.type == SDL_EVENT_MOUSE_MOTION){
				window_to_logical(ev.motion.x, ev.motion.y, mouse_x, mouse_y);
			}else if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT){
				window_to_logical(ev.button.x, ev.button.y, mouse_x, mouse_y);
					ui::v2::Context ctx = make_ctx(/*with_state=*/false);
					// Hit-test consults rect_* for layout-managed buttons,
					// so the same layout pass must run before dispatch.
					if(strcmp(preview_screen, "main_menu") == 0){
						ui::v2::EnsureClayContext(ctx);
						Clay_SetLayoutDimensions(Clay_Dimensions{ (float)ctx.logical_w, (float)ctx.logical_h });
						Clay_BeginLayout();
						ui::v2::RenderMainMenu(ctx, handlers);
						(void)Clay_EndLayout();
						Clay_SetPointerState(Clay_Vector2{ (float)ctx.mouse_x, (float)ctx.mouse_y }, /*pointer_down=*/true);
					}else if(strcmp(preview_screen, "options") == 0){
						ui::v2::EnsureClayContext(ctx);
						Clay_SetLayoutDimensions(Clay_Dimensions{ (float)ctx.logical_w, (float)ctx.logical_h });
						Clay_BeginLayout();
						ui::v2::RenderOptions(ctx, options_handlers);
						(void)Clay_EndLayout();
						Clay_SetPointerState(Clay_Vector2{ (float)ctx.mouse_x, (float)ctx.mouse_y }, /*pointer_down=*/true);
					}else if(strcmp(preview_screen, "options_display") == 0){
						ui::v2::Node tree = ui::v2::BuildOptionsDisplay(ctx, options_display_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "options_audio") == 0){
						ui::v2::Node tree = ui::v2::BuildOptionsAudio(ctx, options_audio_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "options_controls") == 0){
						ui::v2::Node tree = ui::v2::BuildOptionsControls(ctx, options_controls_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_connect") == 0){
						ui::v2::Node tree = ui::v2::BuildLobbyConnect(ctx, lobby_connect_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "lobby") == 0){
						ui::v2::LobbyState lobby_state;
					lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
					ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_create") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameCreate;
						lobby_state.game_create = compute_game_create_state();
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_join") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameJoin;
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_select") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameSelect;
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_tech") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameTech;
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "mission_summary") == 0){
						ui::v2::Node tree = ui::v2::BuildMissionSummary(ctx, mission_summary_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "message_modal") == 0){
						ui::v2::Node tree = ui::v2::BuildMessage(ctx, message_modal_text, /*has_ok=*/true, message_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "password_modal") == 0){
						ui::v2::Node tree = ui::v2::BuildPassword(ctx, password_modal_text, password_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}else if(strcmp(preview_screen, "update") == 0){
						ui::v2::Node tree = ui::v2::BuildUpdate(ctx, update_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClicks(tree, ctx);
					}
			}
		}
		// Measure dt from wallclock so the exponential-approach
		// animations behave the same on faster/slower machines.
		Uint64 now = SDL_GetTicks();
		frame_dt = (last_ticks == 0) ? 0.0f : (float)(now - last_ticks) / 1000.0f;
		last_ticks = now;

		render_once(/*with_state=*/true);
		renderdevice->UploadFrame(screenbuffer.pixels.data(), screenbuffer.w, screenbuffer.h);
		renderdevice->Present();
		SDL_Delay(16);
	}

	return 0;
}
