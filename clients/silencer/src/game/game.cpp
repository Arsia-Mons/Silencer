#include "game.h"

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
	// Fallback: the pre-GPU indexed Surface (TUI/headless or no swapchain capture).
	return renderer.CapturePNG(GetScreenBuffer(), GetPaletteColors(), path);
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
