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

void Game::JoinGame(LobbyGame & lobbygame, char * password) {
gameSession.JoinGame(lobbygame, password);
}

void Game::SpectateGame(LobbyGame & lobbygame, char * password) {
gameSession.SpectateGame(lobbygame, password);
}

void Game::LeaveJoinedGame() {
gameSession.LeaveJoinedGame();
}

bool Game::PushScreen(std::unique_ptr<Screen> s) {
return gameUiPipeline.Push(std::move(s));
}

bool Game::PopScreen() {
return gameUiPipeline.Pop();
}

bool Game::ReplaceScreen(std::unique_ptr<Screen> s) {
return gameUiPipeline.Replace(std::move(s));
}

Screen * Game::GetTopScreen() const {
return gameUiPipeline.Top();
}

bool Game::HasUiInputTarget() {
return gameUiPipeline.HasInputTarget();
}
