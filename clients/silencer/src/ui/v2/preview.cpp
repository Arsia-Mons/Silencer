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
//
// Two impls:
//   * --preview-impl=v2 (default): BuildMainMenu + ui::v2::Render
//   * --preview-impl=legacy      : MainMenuScreen::Build + Renderer::Draw
//
// Two PPM dumps from the same binary (one per impl) feed a byte-diff
// that proves pixel-identical output.

#include "game.h"

#include "context.h"
#include "node.h"
#include "render.h"
#include "main_menu.h"

#include "main_menu_screen.h"
#include "renderer.h"
#include "renderdevice.h"
#include "resources.h"
#include "screen_context.h"
#include "surface.h"
#include "world.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <memory>

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
	const bool use_legacy = (strcmp(preview_impl, "legacy") == 0);
	const char * impl_name = use_legacy ? "legacy" : "v2";

	// Both impls render at logical 640×480. Palette 1 is the menu palette
	// the live game uses for MAINMENU; v2 callers expect it set before
	// render, legacy MainMenuScreen::Build sets it via ResetPresentation
	// but doing it up front is harmless and keeps the dump deterministic
	// even if Build later changes (or v2 forgets to switch).
	if(!renderer.palette.SetPalette(1)){
		fprintf(stderr, "[preview] palette 1 (menu) load failed\n");
		return 4;
	}
	SetColors(renderer.palette.GetColors());

	auto render_once = [&](){
		screenbuffer.Clear(0);
		if(use_legacy){
			if(strcmp(preview_screen, "main_menu") != 0){
				fprintf(stderr, "[preview] unknown screen '%s' for legacy impl\n", preview_screen);
				return;
			}
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
		}else{
			ui::v2::Context ctx{
				world.resources,
				/*logical_w=*/640,
				/*logical_h=*/480,
				/*scale=*/preview_scale,
				/*version=*/SILENCER_VERSION,
			};
			if(strcmp(preview_screen, "main_menu") == 0){
				ui::v2::Node tree = ui::v2::BuildMainMenu(ctx);
				ui::v2::Render(tree, ctx, screenbuffer, renderer);
			}else{
				fprintf(stderr, "[preview] unknown screen '%s' for v2 impl\n", preview_screen);
			}
		}
	};

	render_once();

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
	bool running = true;
	while(running){
		SDL_Event ev;
		while(SDL_PollEvent(&ev)){
			if(ev.type == SDL_EVENT_QUIT) running = false;
			else if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
		}
		render_once();
		renderdevice->UploadFrame(screenbuffer.pixels.data(), screenbuffer.w, screenbuffer.h);
		renderdevice->Present();
		SDL_Delay(16);
	}

	return 0;
}
