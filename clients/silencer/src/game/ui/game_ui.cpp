#include "ui/game_ui_pipeline.h"

#include "game.h"
#include "camera.h"
#include "detonator.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "screen.h"
#include "client/ui/hud/InGameHud.h"
#include "client/ui/hud/InGameOverlays.h"
#include "client/ui/views/HudView.h"
#include "clay_ui_compositor.h"
#include <algorithm>
#include <vector>

namespace {
static const int kLegacyRenderWidth = 640;
static const int kLegacyRenderHeight = 480;

static float GameplayUiScaleForSurface(int width, int height) {
int scaleX = width / kLegacyRenderWidth;
int scaleY = height / kLegacyRenderHeight;
int uiScale = scaleX < scaleY ? scaleX : scaleY;
return static_cast<float>(uiScale > 0 ? uiScale : 1);
}

static float MenuUiScaleForSurface(int width, int height) {
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
} // namespace

GameUiPipeline::GameUiPipeline(Game & g)
: game(g), uiClayService(uiClayBackend), clientUi(uiClayService),
  inGameUiController(g.world), hasPreparedUiInput(false),
  lastUiAnimationMs(0), textInputFocused(false) {
}

bool GameUiPipeline::HasInputTarget() {
if(Top()) return true;
return inGameUiController.HasInputTarget(game.world.localpeerid);
}

void GameUiPipeline::Push(std::unique_ptr<Screen> s){
clientUi.PushScreen(std::move(s), game.screenContext);
}

void GameUiPipeline::Pop(){
clientUi.PopScreen(game.screenContext);
}

void GameUiPipeline::Replace(std::unique_ptr<Screen> s){
clientUi.ReplaceScreen(std::move(s), game.screenContext);
}

Screen * GameUiPipeline::Top() const {
return clientUi.TopScreen();
}

void GameUiPipeline::QueueKeyboardInputForScancode(int sc, const Uint8 * keystate,
                                                   const KeyMap & keymap,
                                                   const GamepadState & gamepadstate) {
clientUiInput.QueueBindingKeyDown(sc);
std::vector<silencer::ui::UiNavAction> queued;
auto queue = [&](silencer::ui::UiNavAction action){
for(auto existing : queued){
if(existing == action) return;
}
clientUiInput.QueueNavAction(action);
queued.push_back(action);
};

switch(sc){
case SDL_SCANCODE_LEFT: queue(silencer::ui::UiNavAction::Left); break;
case SDL_SCANCODE_RIGHT: queue(silencer::ui::UiNavAction::Right); break;
case SDL_SCANCODE_UP: queue(silencer::ui::UiNavAction::Up); break;
case SDL_SCANCODE_DOWN: queue(silencer::ui::UiNavAction::Down); break;
case SDL_SCANCODE_BACKSPACE: queue(silencer::ui::UiNavAction::Backspace); break;
case SDL_SCANCODE_TAB:
if(keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT]){
queue(silencer::ui::UiNavAction::FocusPrevious);
}else{
queue(silencer::ui::UiNavAction::FocusNext);
}
break;
case SDL_SCANCODE_RETURN: queue(silencer::ui::UiNavAction::Confirm); break;
case SDL_SCANCODE_ESCAPE: queue(silencer::ui::UiNavAction::Cancel); break;
default: break;
}

if(keymap.IsPressed(Action::UiUp, keystate, gamepadstate)) queue(silencer::ui::UiNavAction::Up);
if(keymap.IsPressed(Action::UiDown, keystate, gamepadstate)) queue(silencer::ui::UiNavAction::Down);
if(keymap.IsPressed(Action::UiLeft, keystate, gamepadstate)) queue(silencer::ui::UiNavAction::Left);
if(keymap.IsPressed(Action::UiRight, keystate, gamepadstate)) queue(silencer::ui::UiNavAction::Right);
if(keymap.IsPressed(Action::UiConfirm, keystate, gamepadstate)) queue(silencer::ui::UiNavAction::Confirm);
if(keymap.IsPressed(Action::UiCancel, keystate, gamepadstate)) queue(silencer::ui::UiNavAction::Cancel);
}
