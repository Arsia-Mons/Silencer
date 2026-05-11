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
//
// Two impls:
//   * --preview-impl=v2 (default): BuildMainMenu + ui::v2::Render
//   * --preview-impl=legacy      : MainMenuScreen::Build + Renderer::Draw
//
// Two PPM dumps from the same binary (one per impl) feed a byte-diff
// that proves pixel-identical output. PPM dumps leave Context.mouse_x/y
// at default (-1) so no hover state contaminates the comparison.

#include "game.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"
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

#include "main_menu_screen.h"
#include "options_screen.h"
#include "options_display_screen.h"
#include "options_audio_screen.h"
#include "options_controls_screen.h"
#include "lobby_connect_screen.h"
#include "mission_summary_screen.h"
#include "update_screen.h"
#include "message_modal.h"
#include "password_modal.h"
#include "lobby_screen.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_select_panel.h"
#include "game_tech_panel.h"
#include "map_downloader.h"
#include "mapfetch.h"
#include "os.h"
#include "config.h"
#include "overlay.h"
#include "button.h"
#include "interface.h"
#include "objecttypes.h"
#include "team.h"
#include "toggle.h"
#include "textbox.h"
#include "textinput.h"
#include "scrollbar.h"
#include "selectbox.h"
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
	// dispatch below. Lives in src/ui/v2/storybook.cpp.
	if(strcmp(preview_screen, "storybook") == 0){
		return RunStorybook();
	}
	const bool use_legacy = (strcmp(preview_impl, "legacy") == 0);
	const char * impl_name = use_legacy ? "legacy" : "v2";

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
	// — Context defaults to this value, and ButtonHit short-circuits, so
	// the PPM dump path renders without any hover state.
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
		screenbuffer.Clear(0);
		if(use_legacy){
			if(strcmp(preview_screen, "main_menu") == 0){
				// MainMenuScreen::Build creates the widget Object tree on
				// world.objectlist, parks the camera at (320, 240), and
				// re-runs ResetPresentation. Renderer::Draw walks objectlist
				// (the HUD/map paths skip because no map is loaded).
				auto screen = std::make_unique<MainMenuScreen>();
				screen->Build(screenContext);
				// In the live game the loop runs world.Tick() (-> TickObjects)
				// every frame. This advances Object::Tick — Button::Tick sets
				// res_index = 7 for B196x33 chrome on the first call, and
				// Overlay::Tick for bank 208 (the logo) ramps state_i through
				// frames 29..58 then holds at 60. state_i++ runs at the END
				// of Tick, so 61 ticks land state_i at 61 (>= 60 threshold) →
				// res_index = 60, matching v2's static Sprite(208, 60).
				// renderer.Tick() is skipped — it advances ambience / raindrop
				// state we don't want in a deterministic dump.
				for(int i = 0; i < 61; i++){
					world.TickObjects();
				}
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else if(strcmp(preview_screen, "options") == 0){
				// OptionsScreen doesn't call ResetPresentation or
				// SetPosition itself — it inherits the camera from the
				// previous screen (MainMenu parks it at 320, 240 so the
				// menu backdrop draws at logical (0, 0) after the camera
				// offset cancels out). Mirror that here so the standalone
				// preview matches the in-game appearance.
				renderer.camera.SetPosition(320, 240);
				auto screen = std::make_unique<OptionsScreen>();
				screen->Build(screenContext);
				// One TickObjects() is enough — Button::Tick sets
				// res_index = 7 for B196x33 chrome on the first call.
				// No bank-208 logo here so no animation ramp is needed.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else if(strcmp(preview_screen, "options_display") == 0){
				// Same camera inheritance situation as OptionsScreen —
				// the live engine arrives here from OptionsScreen, which
				// inherited (320, 240) from MainMenu.
				renderer.camera.SetPosition(320, 240);
				auto screen = std::make_unique<OptionsDisplayScreen>();
				screen->Build(screenContext);
				// Button chrome (B220x33 + B196x33) settles on its
				// INACTIVE base_index after one tick. The legacy
				// screen Tick (which updates the off/on indicators
				// from Config) is intentionally NOT called — the
				// build-time defaults (off=12, on=14) are the
				// byte-identical target for the v2 build.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else if(strcmp(preview_screen, "options_audio") == 0){
				renderer.camera.SetPosition(320, 240);
				auto screen = std::make_unique<OptionsAudioScreen>();
				screen->Build(screenContext);
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else if(strcmp(preview_screen, "options_controls") == 0){
				// Same camera inheritance as the other options sub-screens
				// — the live engine arrives here from OptionsScreen.
				renderer.camera.SetPosition(320, 240);
				auto screen = std::make_unique<OptionsControlsScreen>();
				screen->Build(screenContext);
				// One TickObjects() settles B112x33 / B220x33 / B196x33
				// chrome res_index on INACTIVE base. The legacy screen Tick
				// (which fills key labels + OR/AND text from keymap) is
				// intentionally NOT called — empty labels are the
				// byte-identical target for the v2 build.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else if(strcmp(preview_screen, "lobby_connect") == 0){
				// Camera inherited from MainMenu/Options chain — same as
				// the other post-MainMenu screens. ResetPresentation(2) in
				// the legacy Build only switches palette + clears, no
				// camera change.
				renderer.camera.SetPosition(320, 240);
				auto screen = std::make_unique<LobbyConnectScreen>();
				screen->Build(screenContext);
				// One TickObjects() settles the Login/Cancel B52x21 buttons
				// on INACTIVE base (effectbrightness=128). The legacy
				// LobbyConnectScreen::Tick is intentionally NOT called: it
				// gates on ambienceMixer.FadedIn() and would mutate the
				// textbox + lobby state, neither of which contribute to
				// the byte-identical target. The build-time
				// iface->ActiveChanged(mouse=false) already set
				// showcaret=true on the active username input, which
				// renderer.Draw consumes to draw the caret rect.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else if(strcmp(preview_screen, "lobby") == 0){
				// Lobby chrome-only preview. The real LobbyScreen::Build also
				// constructs character / chat / gameSelect panels — those are
				// separate items (P10–P15), so inline only the chrome here so
				// the byte-identical PPM target matches the v2 BuildLobby.
				renderer.camera.SetPosition(320, 240);
				::Overlay * background = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				background->res_bank = 7;
				background->res_index = 1;
				::Overlay * toptext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				toptext->text = "Silencer";
				toptext->textbank = 135;
				toptext->textwidth = 11;
				toptext->effectcolor = 152;
				toptext->x = 15;
				toptext->y = 32;
				::Overlay * vertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				vertext->text = "v.";
				vertext->text += world.GetVersion();
				vertext->textbank = 133;
				vertext->textwidth = 6;
				vertext->effectcolor = 189;
				vertext->x = 115;
				vertext->y = 39;
				// mapnametext (uid 8) has empty text at preview gate → no
				// pixels. Skip creating it.
				::Button * exitbutton = static_cast<::Button *>(world.CreateObject(ObjectTypes::BUTTON));
				exitbutton->y = 29;
				exitbutton->x = 473;
				exitbutton->SetType(::Button::B156x21);
				exitbutton->uid = 10;
				strcpy(exitbutton->text, "Go Back");
				::Interface * lobbyiface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				lobbyiface->AddObject(background->id);
				lobbyiface->AddObject(toptext->id);
				lobbyiface->AddObject(vertext->id);
				lobbyiface->AddObject(exitbutton->id);
				lobbyiface->buttonescape = exitbutton->id;

				// Character panel chrome — mirrors CharacterPanel::Build in
				// clients/silencer/src/ui/screens/lobby/panels/character_panel.cpp.
				// Username header + LEVEL/WINS/LOSSES/XP overlays are created
				// but stay empty at preview-gate time (filled in by Tick after
				// a user-info reply lands) so they don't contribute pixels —
				// included here only so the legacy interface tree matches
				// reality. The 5 agency Toggles at (20+i*42, 90) DO render.
				::Interface * characterinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				characterinterface->x = 10;
				characterinterface->y = 64;
				characterinterface->width = 217;
				characterinterface->height = 120;
				::Overlay * usertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				usertext->text = world.lobby.GetLocalUsername();
				usertext->textbank = 134;
				usertext->textwidth = 8;
				usertext->effectcolor = 200;
				usertext->x = 20;
				usertext->y = 71;
				const int statX = 17;
				int statY = 130;
				::Overlay * statOverlays[4] = {nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 4; i++){
					::Overlay * o = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
					o->uid = (Uint8)(2 + i);
					o->textbank = 133;
					o->textwidth = 7;
					o->effectcolor = 129;
					o->effectbrightness = 128 + 32;
					o->textcolorramp = true;
					o->x = statX;
					o->y = statY;
					statY += 13;
					statOverlays[i] = o;
				}
				::Toggle * toggles[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 5; i++){
					::Toggle * t = static_cast<::Toggle *>(world.CreateObject(ObjectTypes::TOGGLE));
					t->y = 90;
					t->x = (Sint16)(20 + i * 42);
					t->res_bank = 181;
					t->res_index = (Uint8)i;
					t->uid = (Uint8)(1 + i);
					t->set = 1;
					if((Uint8)i == Config::GetInstance().defaultagency){
						t->selected = true;
					}
					toggles[i] = t;
				}
				characterinterface->AddObject(usertext->id);
				for(int i = 0; i < 4; i++) characterinterface->AddObject(statOverlays[i]->id);
				for(int i = 0; i < 5; i++) characterinterface->AddObject(toggles[i]->id);
				lobbyiface->AddObject(characterinterface->id);

				// Chat panel — mirrors ChatPanel::Build in
				// clients/silencer/src/ui/screens/lobby/panels/chat_panel.cpp.
				// Channel-name overlay, scrollback / presence textboxes and the
				// chat input all stay empty at preview-gate time so they emit
				// no text pixels; the chat input gets showcaret=true (set by
				// the parent->ActiveChanged below) which renders the caret
				// rect. Scrollbar->draw defaults to false → no scrollbar
				// pixels.
				::Interface * chatinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				chatinterface->x = 15;
				chatinterface->y = 216;
				chatinterface->width = 368;
				chatinterface->height = 234;
				::Overlay * chatborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatborder->res_bank = 7;
				chatborder->res_index = 11;
				::Overlay * chatinputborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatinputborder->res_bank = 7;
				chatinputborder->res_index = 14;
				::Overlay * channeltext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				channeltext->uid = 1;
				channeltext->textbank = 134;
				channeltext->textwidth = 8;
				channeltext->x = 15;
				channeltext->y = 200;
				::TextBox * chattextbox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				chattextbox->x = 19;
				chattextbox->y = 220;
				chattextbox->width = 242;
				chattextbox->height = 207;
				chattextbox->res_bank = 133;
				chattextbox->lineheight = 11;
				chattextbox->fontwidth = 6;
				chattextbox->bottomtotop = true;
				::TextBox * presencebox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				presencebox->x = 267;
				presencebox->y = 220;
				presencebox->width = 110;
				presencebox->height = 207;
				presencebox->res_bank = 133;
				presencebox->lineheight = 11;
				presencebox->fontwidth = 6;
				presencebox->bottomtotop = false;
				presencebox->uid = 9;
				::TextInput * chatinput = static_cast<::TextInput *>(world.CreateObject(ObjectTypes::TEXTINPUT));
				chatinput->x = 18;
				chatinput->y = 437;
				chatinput->width = 360;
				chatinput->height = 14;
				chatinput->res_bank = 133;
				chatinput->fontwidth = 6;
				chatinput->maxchars = 200;
				chatinput->maxwidth = 60;
				chatinput->uid = 1;
				::ScrollBar * chatscrollbar = static_cast<::ScrollBar *>(world.CreateObject(ObjectTypes::SCROLLBAR));
				chatscrollbar->res_index = 12;
				chatscrollbar->barres_index = 13;
				chatscrollbar->scrollpixels = chattextbox->lineheight;
				chatscrollbar->scrollposition = chattextbox->scrolled;
				chatinterface->AddObject(chatborder->id);
				chatinterface->AddObject(chatinputborder->id);
				chatinterface->AddObject(channeltext->id);
				chatinterface->AddObject(chattextbox->id);
				chatinterface->AddObject(presencebox->id);
				chatinterface->AddObject(chatinput->id);
				chatinterface->AddObject(chatscrollbar->id);
				chatinterface->AddTabObject(chatinput->id);
				chatinterface->scrollbar = chatscrollbar->id;
				lobbyiface->AddObject(chatinterface->id);
				lobbyiface->activeobject = chatinterface->id;
				lobbyiface->ActiveChanged(world, lobbyiface, false);

				// One TickObjects() settles the B156x21 Go Back chrome on its
				// INACTIVE base index and runs Toggle::Tick on the 5 agency
				// toggles (effectcolor=112; brightness=128 for the selected
				// one, 32 otherwise). LobbyScreen::Tick / CharacterPanel::Tick
				// are intentionally NOT called — their work (panel dispatch,
				// agency-change detection, stat-text refresh) doesn't
				// contribute to chrome pixels at preview gate.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				lobbyiface->DestroyInterface(world);
			}else if(strcmp(preview_screen, "lobby_create") == 0){
				// Same chrome-only-plus-character-plus-chat scaffolding as
				// the "lobby" branch, then instantiates GameCreatePanel and
				// runs the ShowGameCreate-equivalent activeobject swap so
				// the chat caret turns OFF and the game-create name input
				// caret turns ON.
				renderer.camera.SetPosition(320, 240);
				::Overlay * background = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				background->res_bank = 7;
				background->res_index = 1;
				::Overlay * toptext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				toptext->text = "Silencer";
				toptext->textbank = 135;
				toptext->textwidth = 11;
				toptext->effectcolor = 152;
				toptext->x = 15;
				toptext->y = 32;
				::Overlay * vertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				vertext->text = "v.";
				vertext->text += world.GetVersion();
				vertext->textbank = 133;
				vertext->textwidth = 6;
				vertext->effectcolor = 189;
				vertext->x = 115;
				vertext->y = 39;
				::Button * exitbutton = static_cast<::Button *>(world.CreateObject(ObjectTypes::BUTTON));
				exitbutton->y = 29;
				exitbutton->x = 473;
				exitbutton->SetType(::Button::B156x21);
				exitbutton->uid = 10;
				strcpy(exitbutton->text, "Go Back");
				::Interface * lobbyiface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				lobbyiface->AddObject(background->id);
				lobbyiface->AddObject(toptext->id);
				lobbyiface->AddObject(vertext->id);
				lobbyiface->AddObject(exitbutton->id);
				lobbyiface->buttonescape = exitbutton->id;

				// Character panel — same as the "lobby" branch.
				::Interface * characterinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				characterinterface->x = 10;
				characterinterface->y = 64;
				characterinterface->width = 217;
				characterinterface->height = 120;
				::Overlay * usertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				usertext->text = world.lobby.GetLocalUsername();
				usertext->textbank = 134;
				usertext->textwidth = 8;
				usertext->effectcolor = 200;
				usertext->x = 20;
				usertext->y = 71;
				const int statX_lc = 17;
				int statY_lc = 130;
				::Overlay * statOverlays_lc[4] = {nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 4; i++){
					::Overlay * o = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
					o->uid = (Uint8)(2 + i);
					o->textbank = 133;
					o->textwidth = 7;
					o->effectcolor = 129;
					o->effectbrightness = 128 + 32;
					o->textcolorramp = true;
					o->x = statX_lc;
					o->y = statY_lc;
					statY_lc += 13;
					statOverlays_lc[i] = o;
				}
				::Toggle * toggles_lc[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 5; i++){
					::Toggle * t = static_cast<::Toggle *>(world.CreateObject(ObjectTypes::TOGGLE));
					t->y = 90;
					t->x = (Sint16)(20 + i * 42);
					t->res_bank = 181;
					t->res_index = (Uint8)i;
					t->uid = (Uint8)(1 + i);
					t->set = 1;
					if((Uint8)i == Config::GetInstance().defaultagency){
						t->selected = true;
					}
					toggles_lc[i] = t;
				}
				characterinterface->AddObject(usertext->id);
				for(int i = 0; i < 4; i++) characterinterface->AddObject(statOverlays_lc[i]->id);
				for(int i = 0; i < 5; i++) characterinterface->AddObject(toggles_lc[i]->id);
				lobbyiface->AddObject(characterinterface->id);

				// Chat panel — same as the "lobby" branch.
				::Interface * chatinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				chatinterface->x = 15;
				chatinterface->y = 216;
				chatinterface->width = 368;
				chatinterface->height = 234;
				::Overlay * chatborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatborder->res_bank = 7;
				chatborder->res_index = 11;
				::Overlay * chatinputborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatinputborder->res_bank = 7;
				chatinputborder->res_index = 14;
				::Overlay * channeltext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				channeltext->uid = 1;
				channeltext->textbank = 134;
				channeltext->textwidth = 8;
				channeltext->x = 15;
				channeltext->y = 200;
				::TextBox * chattextbox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				chattextbox->x = 19;
				chattextbox->y = 220;
				chattextbox->width = 242;
				chattextbox->height = 207;
				chattextbox->res_bank = 133;
				chattextbox->lineheight = 11;
				chattextbox->fontwidth = 6;
				chattextbox->bottomtotop = true;
				::TextBox * presencebox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				presencebox->x = 267;
				presencebox->y = 220;
				presencebox->width = 110;
				presencebox->height = 207;
				presencebox->res_bank = 133;
				presencebox->lineheight = 11;
				presencebox->fontwidth = 6;
				presencebox->bottomtotop = false;
				presencebox->uid = 9;
				::TextInput * chatinput = static_cast<::TextInput *>(world.CreateObject(ObjectTypes::TEXTINPUT));
				chatinput->x = 18;
				chatinput->y = 437;
				chatinput->width = 360;
				chatinput->height = 14;
				chatinput->res_bank = 133;
				chatinput->fontwidth = 6;
				chatinput->maxchars = 200;
				chatinput->maxwidth = 60;
				chatinput->uid = 1;
				::ScrollBar * chatscrollbar = static_cast<::ScrollBar *>(world.CreateObject(ObjectTypes::SCROLLBAR));
				chatscrollbar->res_index = 12;
				chatscrollbar->barres_index = 13;
				chatscrollbar->scrollpixels = chattextbox->lineheight;
				chatscrollbar->scrollposition = chattextbox->scrolled;
				chatinterface->AddObject(chatborder->id);
				chatinterface->AddObject(chatinputborder->id);
				chatinterface->AddObject(channeltext->id);
				chatinterface->AddObject(chattextbox->id);
				chatinterface->AddObject(presencebox->id);
				chatinterface->AddObject(chatinput->id);
				chatinterface->AddObject(chatscrollbar->id);
				chatinterface->AddTabObject(chatinput->id);
				chatinterface->scrollbar = chatscrollbar->id;
				lobbyiface->AddObject(chatinterface->id);
				// chatinterface starts active so AddTabObject seeds activeobject
				// = chatinput->id; ShowGameCreate's clear below flips it to 0.
				lobbyiface->activeobject = chatinterface->id;
				lobbyiface->ActiveChanged(world, lobbyiface, false);

				// Now instantiate GameCreatePanel — the actual legacy panel,
				// driven by the same ScreenContext the live game uses.
				// The owner reference is never dereferenced at preview gate
				// (only Tick would use it, and Panel::Tick isn't called), but
				// it has to be a real LobbyScreen instance to avoid UB.
				auto owner = std::make_unique<LobbyScreen>();
				auto gameCreate = std::unique_ptr<GameCreatePanel>(new GameCreatePanel(*owner));
				gameCreate->Build(screenContext, lobbyiface);

				// ShowGameCreate equivalent: focus moves to gameCreate, chat
				// loses its active input → caret toggles between panels.
				lobbyiface->activeobject = gameCreate->interfaceId;
				chatinterface->activeobject = 0;
				lobbyiface->ActiveChanged(world, lobbyiface, false);

				// One TickObjects() settles the B156x21 Go Back / Create
				// chrome on its INACTIVE base + runs Toggle::Tick + leaves
				// every BNONE/textinput button on res_index=0xFF (no chrome).
				// The panel's Tick is intentionally NOT called — its only
				// pixel-affecting work (scrollbar->draw flip when items >
				// height/lineheight) doesn't contribute at preview gate.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				gameCreate->Destroy(screenContext);
				lobbyiface->DestroyInterface(world);
			}else if(strcmp(preview_screen, "lobby_join") == 0){
				// Same chrome+character+chat scaffolding as the "lobby" branch,
				// then instantiates GameJoinPanel. ShowGameJoin (lobby_screen.cpp
				// 355-363) does NOT touch chatiface->activeobject and does NOT
				// re-run lobbyiface->ActiveChanged, so the chat caret stays lit
				// while the join panel is up. Mirror that here by NOT clearing
				// chatinterface->activeobject after building the panel.
				renderer.camera.SetPosition(320, 240);
				::Overlay * background = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				background->res_bank = 7;
				background->res_index = 1;
				::Overlay * toptext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				toptext->text = "Silencer";
				toptext->textbank = 135;
				toptext->textwidth = 11;
				toptext->effectcolor = 152;
				toptext->x = 15;
				toptext->y = 32;
				::Overlay * vertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				vertext->text = "v.";
				vertext->text += world.GetVersion();
				vertext->textbank = 133;
				vertext->textwidth = 6;
				vertext->effectcolor = 189;
				vertext->x = 115;
				vertext->y = 39;
				::Button * exitbutton = static_cast<::Button *>(world.CreateObject(ObjectTypes::BUTTON));
				exitbutton->y = 29;
				exitbutton->x = 473;
				exitbutton->SetType(::Button::B156x21);
				exitbutton->uid = 10;
				strcpy(exitbutton->text, "Go Back");
				::Interface * lobbyiface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				lobbyiface->AddObject(background->id);
				lobbyiface->AddObject(toptext->id);
				lobbyiface->AddObject(vertext->id);
				lobbyiface->AddObject(exitbutton->id);
				lobbyiface->buttonescape = exitbutton->id;

				// Character panel — same as the "lobby" branch.
				::Interface * characterinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				characterinterface->x = 10;
				characterinterface->y = 64;
				characterinterface->width = 217;
				characterinterface->height = 120;
				::Overlay * usertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				usertext->text = world.lobby.GetLocalUsername();
				usertext->textbank = 134;
				usertext->textwidth = 8;
				usertext->effectcolor = 200;
				usertext->x = 20;
				usertext->y = 71;
				const int statX_lj = 17;
				int statY_lj = 130;
				::Overlay * statOverlays_lj[4] = {nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 4; i++){
					::Overlay * o = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
					o->uid = (Uint8)(2 + i);
					o->textbank = 133;
					o->textwidth = 7;
					o->effectcolor = 129;
					o->effectbrightness = 128 + 32;
					o->textcolorramp = true;
					o->x = statX_lj;
					o->y = statY_lj;
					statY_lj += 13;
					statOverlays_lj[i] = o;
				}
				::Toggle * toggles_lj[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 5; i++){
					::Toggle * t = static_cast<::Toggle *>(world.CreateObject(ObjectTypes::TOGGLE));
					t->y = 90;
					t->x = (Sint16)(20 + i * 42);
					t->res_bank = 181;
					t->res_index = (Uint8)i;
					t->uid = (Uint8)(1 + i);
					t->set = 1;
					if((Uint8)i == Config::GetInstance().defaultagency){
						t->selected = true;
					}
					toggles_lj[i] = t;
				}
				characterinterface->AddObject(usertext->id);
				for(int i = 0; i < 4; i++) characterinterface->AddObject(statOverlays_lj[i]->id);
				for(int i = 0; i < 5; i++) characterinterface->AddObject(toggles_lj[i]->id);
				lobbyiface->AddObject(characterinterface->id);

				// Chat panel — same as the "lobby" branch.
				::Interface * chatinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				chatinterface->x = 15;
				chatinterface->y = 216;
				chatinterface->width = 368;
				chatinterface->height = 234;
				::Overlay * chatborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatborder->res_bank = 7;
				chatborder->res_index = 11;
				::Overlay * chatinputborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatinputborder->res_bank = 7;
				chatinputborder->res_index = 14;
				::Overlay * channeltext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				channeltext->uid = 1;
				channeltext->textbank = 134;
				channeltext->textwidth = 8;
				channeltext->x = 15;
				channeltext->y = 200;
				::TextBox * chattextbox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				chattextbox->x = 19;
				chattextbox->y = 220;
				chattextbox->width = 242;
				chattextbox->height = 207;
				chattextbox->res_bank = 133;
				chattextbox->lineheight = 11;
				chattextbox->fontwidth = 6;
				chattextbox->bottomtotop = true;
				::TextBox * presencebox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				presencebox->x = 267;
				presencebox->y = 220;
				presencebox->width = 110;
				presencebox->height = 207;
				presencebox->res_bank = 133;
				presencebox->lineheight = 11;
				presencebox->fontwidth = 6;
				presencebox->bottomtotop = false;
				presencebox->uid = 9;
				::TextInput * chatinput = static_cast<::TextInput *>(world.CreateObject(ObjectTypes::TEXTINPUT));
				chatinput->x = 18;
				chatinput->y = 437;
				chatinput->width = 360;
				chatinput->height = 14;
				chatinput->res_bank = 133;
				chatinput->fontwidth = 6;
				chatinput->maxchars = 200;
				chatinput->maxwidth = 60;
				chatinput->uid = 1;
				::ScrollBar * chatscrollbar = static_cast<::ScrollBar *>(world.CreateObject(ObjectTypes::SCROLLBAR));
				chatscrollbar->res_index = 12;
				chatscrollbar->barres_index = 13;
				chatscrollbar->scrollpixels = chattextbox->lineheight;
				chatscrollbar->scrollposition = chattextbox->scrolled;
				chatinterface->AddObject(chatborder->id);
				chatinterface->AddObject(chatinputborder->id);
				chatinterface->AddObject(channeltext->id);
				chatinterface->AddObject(chattextbox->id);
				chatinterface->AddObject(presencebox->id);
				chatinterface->AddObject(chatinput->id);
				chatinterface->AddObject(chatscrollbar->id);
				chatinterface->AddTabObject(chatinput->id);
				chatinterface->scrollbar = chatscrollbar->id;
				lobbyiface->AddObject(chatinterface->id);
				lobbyiface->activeobject = chatinterface->id;
				lobbyiface->ActiveChanged(world, lobbyiface, false);

				// Now instantiate GameJoinPanel. ShowGameJoin's legacy path
				// only builds the panel — it doesn't touch activeobject —
				// so the chat caret stays lit (driven by the
				// chatinterface->AddTabObject + ActiveChanged above).
				auto owner = std::make_unique<LobbyScreen>();
				auto gameJoin = std::unique_ptr<GameJoinPanel>(new GameJoinPanel(*owner));
				gameJoin->Build(screenContext, lobbyiface);

				// One TickObjects() settles the B156x21 chrome on its
				// INACTIVE base. The panel's Tick (which flips Ready ↔
				// Waiting...) is intentionally NOT called — the legacy Build
				// seeds "Ready" so the byte-identical target has "Ready".
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				gameJoin->Destroy(screenContext);
				lobbyiface->DestroyInterface(world);
			}else if(strcmp(preview_screen, "lobby_select") == 0){
				// Same chrome+character+chat scaffolding as the "lobby" branch,
				// then instantiates GameSelectPanel. ShowGameSelect (lobby_screen.cpp
				// 328-335) does NOT touch chatiface->activeobject and does NOT
				// re-run lobbyiface->ActiveChanged, so the chat caret stays lit
				// while the select panel is up. Mirror that here by NOT clearing
				// chatinterface->activeobject after building the panel.
				renderer.camera.SetPosition(320, 240);
				::Overlay * background = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				background->res_bank = 7;
				background->res_index = 1;
				::Overlay * toptext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				toptext->text = "Silencer";
				toptext->textbank = 135;
				toptext->textwidth = 11;
				toptext->effectcolor = 152;
				toptext->x = 15;
				toptext->y = 32;
				::Overlay * vertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				vertext->text = "v.";
				vertext->text += world.GetVersion();
				vertext->textbank = 133;
				vertext->textwidth = 6;
				vertext->effectcolor = 189;
				vertext->x = 115;
				vertext->y = 39;
				::Button * exitbutton = static_cast<::Button *>(world.CreateObject(ObjectTypes::BUTTON));
				exitbutton->y = 29;
				exitbutton->x = 473;
				exitbutton->SetType(::Button::B156x21);
				exitbutton->uid = 10;
				strcpy(exitbutton->text, "Go Back");
				::Interface * lobbyiface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				lobbyiface->AddObject(background->id);
				lobbyiface->AddObject(toptext->id);
				lobbyiface->AddObject(vertext->id);
				lobbyiface->AddObject(exitbutton->id);
				lobbyiface->buttonescape = exitbutton->id;

				// Character panel — same as the "lobby" branch.
				::Interface * characterinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				characterinterface->x = 10;
				characterinterface->y = 64;
				characterinterface->width = 217;
				characterinterface->height = 120;
				::Overlay * usertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				usertext->text = world.lobby.GetLocalUsername();
				usertext->textbank = 134;
				usertext->textwidth = 8;
				usertext->effectcolor = 200;
				usertext->x = 20;
				usertext->y = 71;
				const int statX_ls = 17;
				int statY_ls = 130;
				::Overlay * statOverlays_ls[4] = {nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 4; i++){
					::Overlay * o = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
					o->uid = (Uint8)(2 + i);
					o->textbank = 133;
					o->textwidth = 7;
					o->effectcolor = 129;
					o->effectbrightness = 128 + 32;
					o->textcolorramp = true;
					o->x = statX_ls;
					o->y = statY_ls;
					statY_ls += 13;
					statOverlays_ls[i] = o;
				}
				::Toggle * toggles_ls[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 5; i++){
					::Toggle * t = static_cast<::Toggle *>(world.CreateObject(ObjectTypes::TOGGLE));
					t->y = 90;
					t->x = (Sint16)(20 + i * 42);
					t->res_bank = 181;
					t->res_index = (Uint8)i;
					t->uid = (Uint8)(1 + i);
					t->set = 1;
					if((Uint8)i == Config::GetInstance().defaultagency){
						t->selected = true;
					}
					toggles_ls[i] = t;
				}
				characterinterface->AddObject(usertext->id);
				for(int i = 0; i < 4; i++) characterinterface->AddObject(statOverlays_ls[i]->id);
				for(int i = 0; i < 5; i++) characterinterface->AddObject(toggles_ls[i]->id);
				lobbyiface->AddObject(characterinterface->id);

				// Chat panel — same as the "lobby" branch.
				::Interface * chatinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				chatinterface->x = 15;
				chatinterface->y = 216;
				chatinterface->width = 368;
				chatinterface->height = 234;
				::Overlay * chatborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatborder->res_bank = 7;
				chatborder->res_index = 11;
				::Overlay * chatinputborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatinputborder->res_bank = 7;
				chatinputborder->res_index = 14;
				::Overlay * channeltext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				channeltext->uid = 1;
				channeltext->textbank = 134;
				channeltext->textwidth = 8;
				channeltext->x = 15;
				channeltext->y = 200;
				::TextBox * chattextbox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				chattextbox->x = 19;
				chattextbox->y = 220;
				chattextbox->width = 242;
				chattextbox->height = 207;
				chattextbox->res_bank = 133;
				chattextbox->lineheight = 11;
				chattextbox->fontwidth = 6;
				chattextbox->bottomtotop = true;
				::TextBox * presencebox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				presencebox->x = 267;
				presencebox->y = 220;
				presencebox->width = 110;
				presencebox->height = 207;
				presencebox->res_bank = 133;
				presencebox->lineheight = 11;
				presencebox->fontwidth = 6;
				presencebox->bottomtotop = false;
				presencebox->uid = 9;
				::TextInput * chatinput = static_cast<::TextInput *>(world.CreateObject(ObjectTypes::TEXTINPUT));
				chatinput->x = 18;
				chatinput->y = 437;
				chatinput->width = 360;
				chatinput->height = 14;
				chatinput->res_bank = 133;
				chatinput->fontwidth = 6;
				chatinput->maxchars = 200;
				chatinput->maxwidth = 60;
				chatinput->uid = 1;
				::ScrollBar * chatscrollbar = static_cast<::ScrollBar *>(world.CreateObject(ObjectTypes::SCROLLBAR));
				chatscrollbar->res_index = 12;
				chatscrollbar->barres_index = 13;
				chatscrollbar->scrollpixels = chattextbox->lineheight;
				chatscrollbar->scrollposition = chattextbox->scrolled;
				chatinterface->AddObject(chatborder->id);
				chatinterface->AddObject(chatinputborder->id);
				chatinterface->AddObject(channeltext->id);
				chatinterface->AddObject(chattextbox->id);
				chatinterface->AddObject(presencebox->id);
				chatinterface->AddObject(chatinput->id);
				chatinterface->AddObject(chatscrollbar->id);
				chatinterface->AddTabObject(chatinput->id);
				chatinterface->scrollbar = chatscrollbar->id;
				lobbyiface->AddObject(chatinterface->id);
				lobbyiface->activeobject = chatinterface->id;
				lobbyiface->ActiveChanged(world, lobbyiface, false);

				// Now instantiate GameSelectPanel. ShowGameSelect's legacy path
				// only builds the panel — it doesn't touch activeobject — so
				// the chat caret stays lit.
				auto owner = std::make_unique<LobbyScreen>();
				auto gameSelect = std::unique_ptr<GameSelectPanel>(new GameSelectPanel(*owner));
				gameSelect->Build(screenContext, lobbyiface);

				// One TickObjects() settles the B156x21 chrome on its INACTIVE
				// base. The panel's Tick (which fills the SelectBox from
				// world.lobby.games + updates the per-game info overlays) is
				// intentionally NOT called — at preview gate the game list is
				// empty and all info overlays stay blank.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				gameSelect->Destroy(screenContext);
				lobbyiface->DestroyInterface(world);
			}else if(strcmp(preview_screen, "lobby_tech") == 0){
				// Same chrome+character+chat scaffolding as the "lobby" branch,
				// then instantiates GameTechPanel and runs the ShowGameTech-
				// equivalent activeobject swap so the chat caret turns OFF.
				// ShowGameTech (lobby_screen.cpp 365-385) clears
				// chatiface->activeobject and re-runs lobbyiface->ActiveChanged,
				// mirroring ShowGameCreate.
				renderer.camera.SetPosition(320, 240);
				::Overlay * background = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				background->res_bank = 7;
				background->res_index = 1;
				::Overlay * toptext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				toptext->text = "Silencer";
				toptext->textbank = 135;
				toptext->textwidth = 11;
				toptext->effectcolor = 152;
				toptext->x = 15;
				toptext->y = 32;
				::Overlay * vertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				vertext->text = "v.";
				vertext->text += world.GetVersion();
				vertext->textbank = 133;
				vertext->textwidth = 6;
				vertext->effectcolor = 189;
				vertext->x = 115;
				vertext->y = 39;
				::Button * exitbutton = static_cast<::Button *>(world.CreateObject(ObjectTypes::BUTTON));
				exitbutton->y = 29;
				exitbutton->x = 473;
				exitbutton->SetType(::Button::B156x21);
				exitbutton->uid = 10;
				strcpy(exitbutton->text, "Go Back");
				::Interface * lobbyiface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				lobbyiface->AddObject(background->id);
				lobbyiface->AddObject(toptext->id);
				lobbyiface->AddObject(vertext->id);
				lobbyiface->AddObject(exitbutton->id);
				lobbyiface->buttonescape = exitbutton->id;

				// Character panel — same as the "lobby" branch.
				::Interface * characterinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				characterinterface->x = 10;
				characterinterface->y = 64;
				characterinterface->width = 217;
				characterinterface->height = 120;
				::Overlay * usertext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				usertext->text = world.lobby.GetLocalUsername();
				usertext->textbank = 134;
				usertext->textwidth = 8;
				usertext->effectcolor = 200;
				usertext->x = 20;
				usertext->y = 71;
				const int statX_lt = 17;
				int statY_lt = 130;
				::Overlay * statOverlays_lt[4] = {nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 4; i++){
					::Overlay * o = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
					o->uid = (Uint8)(2 + i);
					o->textbank = 133;
					o->textwidth = 7;
					o->effectcolor = 129;
					o->effectbrightness = 128 + 32;
					o->textcolorramp = true;
					o->x = statX_lt;
					o->y = statY_lt;
					statY_lt += 13;
					statOverlays_lt[i] = o;
				}
				::Toggle * toggles_lt[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
				for(int i = 0; i < 5; i++){
					::Toggle * t = static_cast<::Toggle *>(world.CreateObject(ObjectTypes::TOGGLE));
					t->y = 90;
					t->x = (Sint16)(20 + i * 42);
					t->res_bank = 181;
					t->res_index = (Uint8)i;
					t->uid = (Uint8)(1 + i);
					t->set = 1;
					if((Uint8)i == Config::GetInstance().defaultagency){
						t->selected = true;
					}
					toggles_lt[i] = t;
				}
				characterinterface->AddObject(usertext->id);
				for(int i = 0; i < 4; i++) characterinterface->AddObject(statOverlays_lt[i]->id);
				for(int i = 0; i < 5; i++) characterinterface->AddObject(toggles_lt[i]->id);
				lobbyiface->AddObject(characterinterface->id);

				// Chat panel — same as the "lobby" branch.
				::Interface * chatinterface = static_cast<::Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
				chatinterface->x = 15;
				chatinterface->y = 216;
				chatinterface->width = 368;
				chatinterface->height = 234;
				::Overlay * chatborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatborder->res_bank = 7;
				chatborder->res_index = 11;
				::Overlay * chatinputborder = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				chatinputborder->res_bank = 7;
				chatinputborder->res_index = 14;
				::Overlay * channeltext = static_cast<::Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
				channeltext->uid = 1;
				channeltext->textbank = 134;
				channeltext->textwidth = 8;
				channeltext->x = 15;
				channeltext->y = 200;
				::TextBox * chattextbox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				chattextbox->x = 19;
				chattextbox->y = 220;
				chattextbox->width = 242;
				chattextbox->height = 207;
				chattextbox->res_bank = 133;
				chattextbox->lineheight = 11;
				chattextbox->fontwidth = 6;
				chattextbox->bottomtotop = true;
				::TextBox * presencebox = static_cast<::TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
				presencebox->x = 267;
				presencebox->y = 220;
				presencebox->width = 110;
				presencebox->height = 207;
				presencebox->res_bank = 133;
				presencebox->lineheight = 11;
				presencebox->fontwidth = 6;
				presencebox->bottomtotop = false;
				presencebox->uid = 9;
				::TextInput * chatinput = static_cast<::TextInput *>(world.CreateObject(ObjectTypes::TEXTINPUT));
				chatinput->x = 18;
				chatinput->y = 437;
				chatinput->width = 360;
				chatinput->height = 14;
				chatinput->res_bank = 133;
				chatinput->fontwidth = 6;
				chatinput->maxchars = 200;
				chatinput->maxwidth = 60;
				chatinput->uid = 1;
				::ScrollBar * chatscrollbar = static_cast<::ScrollBar *>(world.CreateObject(ObjectTypes::SCROLLBAR));
				chatscrollbar->res_index = 12;
				chatscrollbar->barres_index = 13;
				chatscrollbar->scrollpixels = chattextbox->lineheight;
				chatscrollbar->scrollposition = chattextbox->scrolled;
				chatinterface->AddObject(chatborder->id);
				chatinterface->AddObject(chatinputborder->id);
				chatinterface->AddObject(channeltext->id);
				chatinterface->AddObject(chattextbox->id);
				chatinterface->AddObject(presencebox->id);
				chatinterface->AddObject(chatinput->id);
				chatinterface->AddObject(chatscrollbar->id);
				chatinterface->AddTabObject(chatinput->id);
				chatinterface->scrollbar = chatscrollbar->id;
				lobbyiface->AddObject(chatinterface->id);
				lobbyiface->activeobject = chatinterface->id;
				lobbyiface->ActiveChanged(world, lobbyiface, false);

				// Now instantiate GameTechPanel. ShowGameTech clears
				// chatiface->activeobject (caret turns OFF) and reassigns the
				// lobby's activeobject to the tech panel's interface, then
				// re-runs ActiveChanged.
				auto owner = std::make_unique<LobbyScreen>();
				auto gameTech = std::unique_ptr<GameTechPanel>(new GameTechPanel(*owner));
				gameTech->Build(screenContext, lobbyiface);

				// ShowGameTech equivalent.
				lobbyiface->activeobject = gameTech->interfaceId;
				chatinterface->activeobject = 0;
				lobbyiface->ActiveChanged(world, lobbyiface, false);

				// One TickObjects() settles the B156x21 chrome on its INACTIVE
				// base. The panel's Tick (slots-left text refresh / peer-row
				// sync / description-overlay click handling) is intentionally
				// NOT called — at preview gate there is no team/peer/buyable-
				// item state so every dynamic overlay stays empty and only
				// the "Back To Teams" button emits pixels.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				gameTech->Destroy(screenContext);
				lobbyiface->DestroyInterface(world);
			}else if(strcmp(preview_screen, "update") == 0){
				// UpdateScreen reaches from MAINMENU which parked the
				// camera at (320, 240). ResetPresentation(2) in Build only
				// swaps palette + clears, no camera change. Mirror here.
				renderer.camera.SetPosition(320, 240);
				auto screen = std::make_unique<UpdateScreen>();
				screen->Build(screenContext);
				// One TickObjects() settles B156x21 chrome res_index on its
				// INACTIVE base (24). UpdateScreen::Tick — which gates each
				// button's draw on Updater state — is intentionally NOT
				// called: at preview gate all four buttons still draw=true,
				// with three of them stacking at (161, 230) and their text
				// stamped on top of each other in objectlist order
				// (Update, Retry, Download).
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else if(strcmp(preview_screen, "password_modal") == 0){
				// PasswordModal mirrors MessageModal's camera convention —
				// inherits (320, 240) from the MainMenu/Options chain. Mirror
				// for the standalone preview.
				renderer.camera.SetPosition(320, 240);
				auto modal = std::make_unique<PasswordModal>(
				    [](const char *){ /* no-op for preview gate */ });
				modal->Build(screenContext);
				// One TickObjects() settles the B156x21 OK button chrome on
				// its INACTIVE base. The legacy Tick gates on
				// okbutton->clicked and is intentionally NOT called.
				// iface->ActiveChanged(mouse=false) ran in Build → the
				// password input has showcaret=true; renderer.Draw emits the
				// caret rect at (210, 242) since state_i % 32 < 16.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				modal->Destroy(screenContext);
			}else if(strcmp(preview_screen, "message_modal") == 0){
				// MessageModal is normally pushed on top of another screen.
				// Standalone it draws onto the cleared screenbuffer (palette
				// 0 = transparent of the menu palette). Camera inherited from
				// MainMenu/Options chain — same convention as the other
				// post-MainMenu previews.
				renderer.camera.SetPosition(320, 240);
				auto modal = std::make_unique<MessageModal>(message_modal_text);
				modal->Build(screenContext);
				// One TickObjects() settles the B156x21 OK button chrome on
				// its INACTIVE base. MessageModal::Tick — which gates on
				// okbutton->clicked — is intentionally NOT called.
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				modal->Destroy(screenContext);
			}else if(strcmp(preview_screen, "mission_summary") == 0){
				// MissionSummaryScreen::Build calls ResetPresentation(1) +
				// SetPosition(320, 240) itself, but mirror here for safety.
				renderer.camera.SetPosition(320, 240);
				auto screen = std::make_unique<MissionSummaryScreen>();
				screen->Build(screenContext);
				// One TickObjects() settles B196x33 chrome (Done button)
				// res_index on its INACTIVE base. Refresh() ran in Build —
				// at preview gate GetUserInfo just created the User with
				// retrieving=true, so the upgrade-available banner stays
				// draw=false and no +1 upgrade buttons (uid 10..15) get
				// created. The textbox auto-scrolls during AddLine — when
				// text.size() exceeds height/lineheight=27, scrolled is
				// set to text.size()-27. After 60 AddLine calls, scrolled
				// settles at 59-27=32 (the value BEFORE the final
				// push_back). So the visible window starts at deque[32].
				world.TickObjects();
				renderer.Draw(&screenbuffer, /*frametime=*/0);
				screen->Destroy(screenContext);
			}else{
				fprintf(stderr, "[preview] unknown screen '%s' for legacy impl\n", preview_screen);
				return;
			}
		}else{
			ui::v2::Context ctx = make_ctx(with_state);
			if(strcmp(preview_screen, "main_menu") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildMainMenu(ctx, handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
			}else if(strcmp(preview_screen, "options") == 0){
				if(ctx.state) ctx.state->BeginFrame();
				ui::v2::Node tree = ui::v2::BuildOptions(ctx, options_handlers);
				ui::v2::Layout(tree, ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
				if(ctx.state) ctx.state->EndFrame();
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
		printf("[preview] %s impl, screen=%s, scale=%d -> %s\n",
		       impl_name, preview_screen, preview_scale, dump_ppm_path);
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
	printf("[preview] interactive %s, %s, scale=%d  (Esc/close to exit)\n",
	       impl_name, preview_screen, preview_scale);

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
				if(!use_legacy){
					ui::v2::Context ctx = make_ctx(/*with_state=*/false);
					// Hit-test consults rect_* for layout-managed buttons,
					// so the same layout pass must run before dispatch.
					if(strcmp(preview_screen, "main_menu") == 0){
						ui::v2::Node tree = ui::v2::BuildMainMenu(ctx, handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "options") == 0){
						ui::v2::Node tree = ui::v2::BuildOptions(ctx, options_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "options_display") == 0){
						ui::v2::Node tree = ui::v2::BuildOptionsDisplay(ctx, options_display_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "options_audio") == 0){
						ui::v2::Node tree = ui::v2::BuildOptionsAudio(ctx, options_audio_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "options_controls") == 0){
						ui::v2::Node tree = ui::v2::BuildOptionsControls(ctx, options_controls_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_connect") == 0){
						ui::v2::Node tree = ui::v2::BuildLobbyConnect(ctx, lobby_connect_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "lobby") == 0){
						ui::v2::LobbyState lobby_state;
					lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
					ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_create") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameCreate;
						lobby_state.game_create = compute_game_create_state();
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_join") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameJoin;
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_select") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameSelect;
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "lobby_tech") == 0){
						ui::v2::LobbyState lobby_state;
						lobby_state.character.selected_agency = Config::GetInstance().defaultagency;
						lobby_state.active_panel = ui::v2::LobbyActivePanel::GameTech;
						ui::v2::Node tree = ui::v2::BuildLobby(ctx, lobby_handlers, lobby_state);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "mission_summary") == 0){
						ui::v2::Node tree = ui::v2::BuildMissionSummary(ctx, mission_summary_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "message_modal") == 0){
						ui::v2::Node tree = ui::v2::BuildMessage(ctx, message_modal_text, /*has_ok=*/true, message_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "password_modal") == 0){
						ui::v2::Node tree = ui::v2::BuildPassword(ctx, password_modal_text, password_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}else if(strcmp(preview_screen, "update") == 0){
						ui::v2::Node tree = ui::v2::BuildUpdate(ctx, update_handlers);
						ui::v2::Layout(tree, ctx);
						ui::v2::DispatchClick(tree, ctx);
					}
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
