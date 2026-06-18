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
		dev->RequestCapture();
		gameRenderer.Present();
		std::vector<Uint8> rgba; int w = 0, h = 0;
		if(dev->TakeCapturedFrame(rgba, w, h) && !rgba.empty()){
			return renderer.WriteRGBAPNG(rgba.data(), w, h, path);
		}
	}
	// Headless fallback: composite the premultiplied cppx UI RGBA over the
	// opaque palettized world on the CPU (out = src + dst*(1-srcA)).
	const Surface & buf = GetScreenBuffer();
	const SDL_Color * palette = GetPaletteColors();
	int uw = 0;
	int uh = 0;
	const Uint8 * ui = gameUiPipeline.CppxUiFrame(uw, uh);
	// Mirror the GPU screen fade: dim UI RGB only, branching on the ORIGINAL
	// alpha so coverage is kept and the HUD dims to black (not translucent).
	// 255 == no-op.
	const Uint32 uia = (Uint32)(gameRenderer.UiFadeAlpha() * 255.0f + 0.5f);
	auto dim = [uia](int v) -> int { return uia >= 255 ? v : (int)((v * uia + 127u) / 255u); };
	if(ui && uw == buf.w && uh == buf.h && palette){
		std::vector<Uint8> rgba(static_cast<size_t>(buf.w) * buf.h * 4);
		for(int i = 0; i < buf.w * buf.h; ++i){
			SDL_Color c = palette[buf.pixels[i]];
			int sa = ui[i * 4 + 3];
			int inv = 255 - sa;
			if(sa > 0 && sa < 255){
				// Translucent UI over the world must mix in PALETTE space (origin's
				// alpha LUT), not linear RGB — linear diverges by up to half a step.
				SDL_Color sc = {
					static_cast<Uint8>(ui[i * 4 + 0] * 255 / sa),
					static_cast<Uint8>(ui[i * 4 + 1] * 255 / sa),
					static_cast<Uint8>(ui[i * 4 + 2] * 255 / sa), 255};
				// origin authors translucent fills as palette INDICES; black is
				// index 0 (the LUT's low rows are real mixes), so map it straight.
				Uint8 si = (sc.r | sc.g | sc.b) == 0
					? 0 : renderer.palette.ClosestMatch(sc);
				SDL_Color mixed = palette[renderer.palette.Alpha(si, buf.pixels[i])];
				rgba[i * 4 + 0] = mixed.r;
				rgba[i * 4 + 1] = mixed.g;
				rgba[i * 4 + 2] = mixed.b;
				rgba[i * 4 + 3] = 255;
				continue;
			}
			rgba[i * 4 + 0] = static_cast<Uint8>(dim(ui[i * 4 + 0]) + c.r * inv / 255);
			rgba[i * 4 + 1] = static_cast<Uint8>(dim(ui[i * 4 + 1]) + c.g * inv / 255);
			rgba[i * 4 + 2] = static_cast<Uint8>(dim(ui[i * 4 + 2]) + c.b * inv / 255);
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

void Game::LeaveMatchToMenu() {
world.Disconnect();
if(world.lobby.state == Lobby::AUTHENTICATED){
GoToState(GameState::LOBBY);
world.lobby.JoinChannel(world.lobby.lastchannel);
}else{
if(world.replay.IsPlaying()){
world.replay.EndPlaying();
}
GoToState(GameState::MAINMENU);
}
}
