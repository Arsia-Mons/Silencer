#include "screen_context.h"

#include "ambience_mixer.h"
#include "config.h"
#include "game.h"
#include "keybinds.h"
#include "renderer.h"
#include "screen.h"
#include "modal.h"
#include "message_modal.h"
#include "surface.h"
#include "runtime/UiInteractionRegistry.h"
#include "lobby.h"
#include "lobbygame.h"
#include "renderdevice.h"
#include "runtime/UiActionQueue.h"
#include "updater.h"
#include "updaterstage2.h"
#include "world.h"

#include <SDL3/SDL_video.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{
ScreenContext::UpdateState ToScreenUpdateState(Updater::State state)
{
	switch(state){
		case Updater::IDLE:
			return ScreenContext::UpdateState::Idle;
		case Updater::PROMPTING:
			return ScreenContext::UpdateState::Prompting;
		case Updater::DOWNLOADING:
			return ScreenContext::UpdateState::Downloading;
		case Updater::VERIFYING:
			return ScreenContext::UpdateState::Verifying;
		case Updater::STAGING:
			return ScreenContext::UpdateState::Staging;
		case Updater::FAILED:
			return ScreenContext::UpdateState::Failed;
		case Updater::DONE:
			return ScreenContext::UpdateState::Done;
	}
	assert(false && "Unhandled updater state");
	std::abort();
}
} // namespace

ScreenContext::ScreenContext(Game & game_,
                             World & world_,
                             Renderer & renderer_,
                             Lobby & lobby_,
                             KeyMap & keymap_,
                             Updater & updater_,
                             AmbienceMixer & ambienceMixer_,
                             MapDownloader & mapDownloader_,
                             SDL_Window * & window_,
                             RenderDevice * & renderdevice_)
    : game(game_),
      renderer(renderer_),
      keymap(keymap_),
      updater(updater_),
      ambienceMixer(ambienceMixer_),
      window(window_),
      renderdevice(renderdevice_),
      world(world_),
      lobby(lobby_),
      mapDownloader(mapDownloader_)
{
}

void ScreenContext::GoToState(Uint8 newState) { game.GoToState(newState); }
bool ScreenContext::GoBack() { return game.GoBack(); }
void ScreenContext::RequestQuit() { game.quitRequested = true; }
void ScreenContext::LeaveJoinedGame() { game.LeaveJoinedGame(); }
bool ScreenContext::IsJoiningGame() const { return game.joininggame; }
void ScreenContext::SetJoiningGame(bool joining) { game.joininggame = joining; }
bool ScreenContext::IsCreateGamePending() const { return game.creategameclicked; }
void ScreenContext::SetCreateGamePending(bool pending) { game.creategameclicked = pending; }
LobbyGame * ScreenContext::FindLobbyGame(Uint32 gameId) const { return world.lobby.GetGameById(gameId); }
LobbyGame * ScreenContext::CurrentLobbyGame() const { return FindLobbyGame(game.currentlobbygameid); }
void ScreenContext::JoinLobbyGame(LobbyGame & lobbyGame, char * password) {
	game.currentlobbygameid = lobbyGame.id;
	game.JoinGame(lobbyGame, password);
}
void ScreenContext::SpectateLobbyGame(LobbyGame & lobbyGame, char * password) {
	game.currentlobbygameid = lobbyGame.id;
	game.SpectateGame(lobbyGame, password);
}
SDL_GamepadType ScreenContext::CurrentGamepadType() const {
	SDL_Gamepad * pad = game.GetGamepad();
	return pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
}
bool ScreenContext::PushScreen(std::unique_ptr<Screen> s) { return game.PushScreen(std::move(s)); }
bool ScreenContext::PopScreen() { return game.PopScreen(); }
bool ScreenContext::ReplaceScreen(std::unique_ptr<Screen> s) { return game.ReplaceScreen(std::move(s)); }
Screen * ScreenContext::TopScreen() const { return game.GetTopScreen(); }
bool ScreenContext::ShowModal(std::unique_ptr<Modal> m) {
	return game.PushScreen(std::unique_ptr<Screen>(static_cast<Screen *>(m.release())));
}

bool ScreenContext::ShowMessage(const char * msg, std::function<void()> onClose) {
	return game.PushScreen(std::make_unique<MessageModal>(msg ? msg : "", std::move(onClose)));
}

void ScreenContext::ClearUiFocus() {
	game.UiInteractions().ClearFocus();
}

std::string ScreenContext::KeybindPresetText() const {
	if(!keymap.label.empty()) return keymap.label;
	if(!keymap.name.empty()) return keymap.name;
	return Config::GetInstance().active_keybind_profile;
}

void ScreenContext::CycleKeybindPreset() {
	::CycleKeybindPreset(keymap);
}

void ScreenContext::SaveActiveKeybindProfileIfCustom() {
	const std::string active = Config::GetInstance().active_keybind_profile;
	if(active == "default" || active == "wasd" || active == "gamepad") return;
	keymap.SaveFile(WritableProfilePath(active));
}

void ScreenContext::ReloadActiveKeymap() {
	LoadActiveKeymap(keymap);
}

ScreenContext::LegacyKeyBindingSlots ScreenContext::LegacyKeyBinding(Action action) const {
	LegacyKeyBindingSlots out;
	const auto & ab = keymap.Get(action);
	if(ab.bindings.empty()) return out;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		out.key1 = static_cast<SDL_Scancode>(b0.keys[0].code);
		out.key2 = static_cast<SDL_Scancode>(b0.keys[1].code);
		out.and_ = true;
		return out;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		out.key1 = static_cast<SDL_Scancode>(b0.keys[0].code);
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			out.key2 = static_cast<SDL_Scancode>(b1.keys[0].code);
		}
	}
	return out;
}

void ScreenContext::WriteLegacyKeyBinding(Action action,
                                          SDL_Scancode key1,
                                          SDL_Scancode key2,
                                          bool and_) {
	ForkActiveProfileIfBuiltin(keymap);
	auto & ab = keymap.Get(action);
	ab.bindings.clear();
	auto mk = [](SDL_Scancode sc){
		BindingKey k;
		k.device  = BindingDevice::Keyboard;
		k.code    = static_cast<int>(sc);
		k.axisDir = 0;
		return k;
	};
	if(key1 == SDL_SCANCODE_UNKNOWN && key2 == SDL_SCANCODE_UNKNOWN) return;
	if(and_ && key1 != SDL_SCANCODE_UNKNOWN && key2 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key1));
		b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
		return;
	}
	if(key1 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key1));
		ab.bindings.push_back(std::move(b));
	}
	if(key2 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
	}
}

std::string ScreenContext::KeyBindingSlotLabel(Action action, int slot) const {
	const auto & ab = keymap.Get(action);
	int found = 0;
	for(const auto & b : ab.bindings){
		if(b.keys.empty()) continue;
		if(found == slot){
			const auto & k = b.keys[0];
			if(k.device == BindingDevice::Keyboard){
				return KeyMap::GetKeyName(static_cast<SDL_Scancode>(k.code));
			}
			std::string s = Stringify(k);
			auto colon = s.find(':');
			std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
			return GamepadShortLabel(raw, CurrentGamepadType());
		}
		found++;
	}
	return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
}

bool ScreenContext::SetCapturedBinding(Action action, int slot, const silencer::ui::UiBindingInput & input) {
	BindingKey bindingKey{};
	if(input.kind == silencer::ui::UiBindingInputKind::GamepadButtonDown){
		bindingKey.device = BindingDevice::GamepadButton;
		bindingKey.code = input.code;
		bindingKey.axisDir = 0;
	}else if(input.kind == silencer::ui::UiBindingInputKind::GamepadAxisMoved){
		bindingKey.device = BindingDevice::GamepadAxis;
		bindingKey.code = input.code;
		bindingKey.axisDir = static_cast<int8_t>(input.axisDir < 0 ? -1 : 1);
	}else{
		return false;
	}

	ForkActiveProfileIfBuiltin(keymap);
	auto & ab = keymap.Get(action);
	Binding binding;
	binding.keys.push_back(bindingKey);
	if(slot == 0){
		if(ab.bindings.empty()) ab.bindings.push_back(binding);
		else ab.bindings[0] = binding;
	}else{
		if(ab.bindings.empty()) ab.bindings.push_back(Binding{});
		if(ab.bindings.size() < 2) ab.bindings.push_back(binding);
		else ab.bindings[1] = binding;
	}
	return true;
}

void ScreenContext::ResetPresentation(int paletteIdx) {
	renderer.palette.SetPalette(paletteIdx);
	game.GetScreenBuffer().Clear(0);
	game.gameRenderer.SetColors(renderer.palette.GetColors());
}

void ScreenContext::ResetMenuPresentation(int paletteIdx) {
	ResetPresentation(paletteIdx);
	renderer.camera.SetPosition(320, 240);
}

bool ScreenContext::UiBlinkVisible() const {
	return (renderer.GetHudAnimationPhase() % 32) < 16;
}

std::string ScreenContext::ClientVersion() const {
	return world.GetVersion();
}

ScreenContext::UiSpriteFrameMetrics
ScreenContext::GetUiSpriteFrameMetrics(Uint8 bank, Uint16 index) const {
	const Resources & resources = world.resources;
	UiSpriteFrameMetrics metrics;
	metrics.offsetX = resources.spriteoffsetx[bank][index];
	metrics.offsetY = resources.spriteoffsety[bank][index];
	metrics.width = static_cast<int>(resources.spritewidth[bank][index]);
	metrics.height = static_cast<int>(resources.spriteheight[bank][index]);
	return metrics;
}

Uint32 ScreenContext::WorldTickCount() const {
	return world.tickcount;
}

bool ScreenContext::LobbyMusicFadedIn() const {
	return ambienceMixer.FadedIn();
}

std::string ScreenContext::LobbyGameChannelName(LobbyGame & lobbyGame) {
	char name[256];
	ambienceMixer.GetGameChannelName(lobbyGame, name);
	return name;
}

void ScreenContext::PresentUpdate(const std::string & url, const uint8_t sha256[32]) {
	updater.PresentUpdate(url, sha256);
}

ScreenContext::UpdateState ScreenContext::CurrentUpdateState() {
	return ToScreenUpdateState(updater.GetState());
}

float ScreenContext::UpdateProgress() {
	return updater.GetProgress();
}

std::string ScreenContext::UpdateErrorMessage() {
	return updater.GetErrorMessage();
}

int ScreenContext::UpdateRetryCount() {
	return updater.GetRetryCount();
}

void ScreenContext::ConsentUpdate() {
	updater.Consent();
}

void ScreenContext::CancelUpdate() {
	updater.Cancel();
}

void ScreenContext::RetryUpdate() {
	updater.Retry();
}

void ScreenContext::OpenUpdateDownloadPage() {
	std::string url = updater.GetDownloadURL();
#ifdef _WIN32
	std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
	std::string cmd = "open '" + url + "'";
#else
	std::string cmd = "xdg-open '" + url + "' &";
#endif
	system(cmd.c_str());
}

bool ScreenContext::LaunchStagedUpdate() {
	std::string zippath =
#ifdef _WIN32
		std::string(getenv("TEMP") ? getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
		"/tmp/silencer-update.zip";
#endif
	fprintf(stderr, "[updater] UpdateScreen invoking UpdaterStage2::Launch with zip=%s\n",
	        zippath.c_str());
	if(UpdaterStage2::Launch(zippath)){
		updater.MarkStage2Spawned();
		return true;
	}
	fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
	return false;
}

void ScreenContext::SetWindowFullscreen(bool fullscreen) {
	if(window) SDL_SetWindowFullscreen(window, fullscreen);
}

void ScreenContext::SetScaleFilter(bool enabled) {
	if(renderdevice) renderdevice->SetScaleFilter(enabled);
}

void ScreenContext::BeginLobbyPanelBorderBlur(int width, int height, float uiScale) {
	if(renderdevice) renderdevice->BeginLobbyPanelBorderBlur(width, height, uiScale);
}

void ScreenContext::AddLobbyPanelBorderBlurRect(int x, int y, int w, int h) {
	if(!renderdevice || w <= 0 || h <= 0) return;
	renderdevice->AddLobbyPanelBorderBlurRect(SDL_Rect{ x, y, w, h });
}
