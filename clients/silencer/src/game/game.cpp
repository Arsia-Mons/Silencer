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

void Game::ResetPresentationPalette(int paletteIdx) {
renderer.palette.SetPalette(paletteIdx);
GetScreenBuffer().Clear(0);
gameRenderer.SetColors(renderer.palette.GetColors());
}

void Game::Present() {
gameRenderer.Present();
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

void Game::StartTutorial() {
GoToState(GameState::SINGLEPLAYERGAME);
}

bool Game::HasUiInputTarget() {
return gameUiPipeline.HasInputTarget();
}
