#include "game.h"
#include "screen.h"

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

bool Game::SetupRenderDevice() {
return gameRenderer.Setup(&gameRenderer.WindowRef());
}

void Game::SetColors(SDL_Color * colors) {
gameRenderer.SetColors(colors);
}

void Game::UpdateInputState(Input & input) {
gameInput.UpdateInputState(input);
}

bool Game::LoadMap(const char * name) {
return gameSession.LoadMap(name);
}

void Game::UnloadGame() {
gameSession.UnloadGame();
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

void Game::ShowDeployMessage() {
gameSession.ShowDeployMessage();
}

void Game::GiveDefaultItems(Player & player) {
gameSession.GiveDefaultItems(player);
}

void Game::RestartPaletteFade() {
gameRenderer.RestartPaletteFade();
}

float Game::LegacyUiAnimationStepSeconds() const {
return gameRenderer.LegacyUiAnimationStepSeconds();
}

Uint8 Game::PaletteFadePhaseFromClock() const {
return gameRenderer.PaletteFadePhaseFromClock();
}

bool Game::PaletteFadeFinished() const {
return gameRenderer.PaletteFadeFinished();
}

void Game::ApplyPaletteFade(bool fadeOut) {
gameRenderer.ApplyPaletteFade(fadeOut);
}

void Game::PrepareClientUiFrame(Surface& surface) {
gameUiPipeline.PrepareFrame(surface);
}

void Game::BeginPreparedClientUiFrame() {
gameUiPipeline.BeginFrame();
}

Clay_RenderCommandArray Game::EndClientUiFrame() {
return gameUiPipeline.EndFrame();
}

void Game::RenderClientUiFrame(Surface& surface, float frametime) {
gameUiPipeline.RenderFrame(surface, frametime);
}

void Game::ResetUiFrameDeltas() {
gameUiPipeline.ResetDeltas();
}

void Game::BuildVisibleClientUi(Surface& surface, float frametime) {
gameUiPipeline.BuildVisible(surface, frametime);
}

void Game::DrawInGameWorldInsets(Surface& surface, float frametime) {
gameUiPipeline.DrawInGameWorldInsets(surface, frametime);
}

void Game::PushScreen(std::unique_ptr<Screen> s) {
gameUiPipeline.Push(std::move(s));
}

void Game::PopScreen() {
gameUiPipeline.Pop();
}

void Game::ReplaceScreen(std::unique_ptr<Screen> s) {
gameUiPipeline.Replace(std::move(s));
}

Screen * Game::GetTopScreen() const {
return gameUiPipeline.Top();
}

bool Game::HasUiInputTarget() {
return gameUiPipeline.HasInputTarget();
}

void Game::OpenFirstGamepad() {
gameInput.OpenFirstGamepad();
}

void Game::PollGamepadState() {
gameInput.PollGamepadState();
}

void Game::OnScancodeDown(int scancode) {
gameInput.OnScancodeDown(scancode);
}

void Game::OnScancodeUp(int scancode) {
gameInput.OnScancodeUp(scancode);
}

void Game::QueueUiKeyboardInputForScancode(int scancode) {
gameUiPipeline.QueueKeyboardInputForScancode(
scancode,
gameInput.GetKeystate(),
gameInput.GetKeyMap(),
gameInput.GetGamepadState());
}

void Game::TickGamepadMenuNav() {
gameInput.TickGamepadMenuNav();
}

void Game::TickRumble() {
gameInput.TickRumble(world.GetPeerPlayer(world.localpeerid));
}

const char * Game::GetActionKeyDisplayName(Action a) {
return gameInput.GetActionKeyDisplayName(a);
}
