#include "game.h"
#include "gasloader.h"
#include <algorithm>
#include <cstring>

using namespace GameState;

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
