#include "render/game_renderer.h"

#include "game.h"
#include "config.h"
#include "gasloader.h"
#include "sdl3gpubackend.h"
#include "tuibackend.h"
#include <algorithm>
#include <cstring>

namespace {
static const int kLegacyRenderWidth = 640;
static const int kLegacyRenderHeight = 480;
}

GameRenderer::GameRenderer(Game & g)
: game(g), renderdevice(nullptr), screenbuffer(640, 480), window(nullptr), fade_i(0), fadeStartMs(0) {
std::memset(palettecolors, 0, sizeof(palettecolors));
}

bool GameRenderer::Setup(SDL_Window ** outWindow) {
if(outWindow) {
window = *outWindow;
}
if(game.tui){
TUIBackend * backend = new TUIBackend();
if(!backend->Init(nullptr)){
delete backend;
return false;
}
renderdevice = backend;
renderdevice->SetScaleFilter(false);
}else{
SDL3GPUBackend * backend = new SDL3GPUBackend();
if(!backend->Init(window)){
delete backend;
return false;
}
renderdevice = backend;
renderdevice->SetScaleFilter(Config::GetInstance().scalefilter);
}
if(outWindow) {
*outWindow = window;
}
return true;
}

bool GameRenderer::ResizeRenderSurfacePixels(int width, int height){
if(width < 1 || height < 1) return false;
if(screenbuffer.w == width && screenbuffer.h == height) return true;
screenbuffer.Resize(width, height, 0);
return true;
}

bool GameRenderer::SyncRenderSurfaceToWindowPixels(){
if(game.world.map.loaded){
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

bool GameRenderer::ResizeRenderSurface(int width, int height){
if(width < 1 || height < 1) return false;
if(window){
SDL_SetWindowSize(window, width, height);
if(game.world.map.loaded){
return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
}
return SyncRenderSurfaceToWindowPixels();
}
if(game.world.map.loaded){
return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
}
return ResizeRenderSurfacePixels(width, height);
}

void GameRenderer::Present(){
if(renderdevice){
renderdevice->UploadFrame(screenbuffer.pixels.data(), screenbuffer.w, screenbuffer.h);
renderdevice->Present();
}
}

void GameRenderer::LoadProgressCallback(int progress, int totalprogressitems){
if(game.world.dedicatedserver.active){
return;
}
game.HandleSDLEvents();
if(SDL_GetTicks() - game.lasttick >= 100){
int width = std::min(500, screenbuffer.w - 32);
int widthp = (float(progress) / totalprogressitems) * width;
int barx = (screenbuffer.w - width) / 2;
int bary = (screenbuffer.h - 32) / 2;
game.renderer.DrawFilledRectangle(&screenbuffer, barx, bary, barx + width, bary + 32, 101);
if(widthp > 0){
for(int c = 0; c < 13; c++){
int x0 = barx + (c * widthp) / 13;
int x1 = barx + ((c + 1) * widthp) / 13;
if(x1 > x0) game.renderer.DrawFilledRectangle(&screenbuffer, x0, bary, x1, bary + 32, 101 + c);
}
}
Present();
game.lasttick = SDL_GetTicks();
}
}

void GameRenderer::SetColors(SDL_Color * colors){
std::memcpy(palettecolors, colors, 256 * sizeof(SDL_Color));
if(renderdevice){
renderdevice->SetPalette(colors, 256);
}
}

void GameRenderer::RestartPaletteFade(){
fadeStartMs = SDL_GetTicks();
fade_i = 0;
}

float GameRenderer::LegacyUiAnimationStepSeconds() const {
const int hz = GASLoader::Get().gameengine.ticksPerSecond > 0
? GASLoader::Get().gameengine.ticksPerSecond
: 24;
return 1.0f / static_cast<float>(hz);
}

Uint8 GameRenderer::PaletteFadePhaseFromClock() const {
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

bool GameRenderer::PaletteFadeFinished() const {
return PaletteFadePhaseFromClock() >= 16;
}

void GameRenderer::ApplyPaletteFade(bool fadeOut){
fade_i = PaletteFadePhaseFromClock();
int phase = fade_i;
if(phase > 15) phase = 15;
if(fadeOut){
SDL_Color * fadedpalette =
game.renderer.palette.CopyWithBrightness(game.renderer.palette.GetColors(), (15 - phase) * 8);
SetColors(fadedpalette);
return;
}
if(phase >= 15){
SetColors(game.renderer.palette.GetColors());
return;
}
SDL_Color * fadedpalette =
game.renderer.palette.CopyWithBrightness(game.renderer.palette.GetColors(), phase * 8);
SetColors(fadedpalette);
}
