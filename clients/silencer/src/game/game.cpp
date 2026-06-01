#include "game.h"

#include <vector>

void Game::LoadProgressCallback(int progress, int totalprogressitems) {
gameRenderer.LoadProgressCallback(progress, totalprogressitems);
}

bool Game::ResizeRenderSurface(int width, int height) {
return gameRenderer.ResizeRenderSurface(width, height);
}

bool Game::ResizeRenderSurfacePixels(int width, int height) {
return gameRenderer.ResizeRenderSurfacePixels(width, height);
}

bool Game::SyncRenderSurfaceToWindowPixels() {
return gameRenderer.SyncRenderSurfaceToWindowPixels();
}

void Game::Present() {
gameRenderer.Present();
}

bool Game::CaptureCompositedFrame(const char * path) {
	RenderDevice * dev = gameRenderer.GetRenderDevice();
	if(dev){
		// Arm + render one frame so the device downloads the final composited
		// swapchain (world + cppx UI overlay). Textures retain the last upload,
		// so this re-presents the same frame.
		dev->RequestCapture();
		gameRenderer.Present();
		std::vector<Uint8> rgba; int w = 0, h = 0;
		if(dev->TakeCapturedFrame(rgba, w, h) && !rgba.empty()){
			return renderer.WriteRGBAPNG(rgba.data(), w, h, path);
		}
	}
	// Fallback (headless / no swapchain capture): composite the cppx UI RGBA over
	// the palettized world frame on the CPU, so headless screenshots still show
	// the UI the GPU would otherwise composite at present. The cppx layer is
	// premultiplied and rendered at the world-surface size in headless, so it
	// over-blends 1:1 onto the opaque world (out = src + dst*(1-srcA)).
	const Surface & buf = GetScreenBuffer();
	const SDL_Color * palette = GetPaletteColors();
	int uw = 0;
	int uh = 0;
	const Uint8 * ui = gameUiPipeline.CppxUiFrame(uw, uh);
	if(ui && uw == buf.w && uh == buf.h && palette){
		std::vector<Uint8> rgba(static_cast<size_t>(buf.w) * buf.h * 4);
		for(int i = 0; i < buf.w * buf.h; ++i){
			SDL_Color c = palette[buf.pixels[i]];
			int sa = ui[i * 4 + 3];
			int inv = 255 - sa;
			rgba[i * 4 + 0] = static_cast<Uint8>(ui[i * 4 + 0] + c.r * inv / 255);
			rgba[i * 4 + 1] = static_cast<Uint8>(ui[i * 4 + 1] + c.g * inv / 255);
			rgba[i * 4 + 2] = static_cast<Uint8>(ui[i * 4 + 2] + c.b * inv / 255);
			rgba[i * 4 + 3] = 255;
		}
		return renderer.WriteRGBAPNG(rgba.data(), buf.w, buf.h, path);
	}
	return renderer.CapturePNG(buf, palette, path);
}

void Game::JoinGame(LobbyGame & lobbygame, char * password) {
gameSession.JoinGame(lobbygame, password);
}

void Game::SpectateGame(LobbyGame & lobbygame, char * password) {
gameSession.SpectateGame(lobbygame, password);
}

void Game::LeaveJoinedGame() {
gameSession.LeaveJoinedGame();
}
