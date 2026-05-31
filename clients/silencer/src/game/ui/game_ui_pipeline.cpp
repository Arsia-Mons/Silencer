#include "ui/game_ui_pipeline.h"

#include "client/ui/screens/screen.h"
#include "character_create_screen.h"
#include "game.h"
#include "camera.h"
#include "detonator.h"
#include "gasloader.h"
#include "game_state.h"
#include "lobby_connect_screen.h"
#include "main_menu_screen.h"
#include "mission_summary_screen.h"
#include "objecttypes.h"
#include "options_audio_screen.h"
#include "options_controls_screen.h"
#include "options_display_screen.h"
#include "options_screen.h"
#include "player.h"
#include "update_screen.h"
#include "client/ui/views/HudView.h"
#ifdef SILENCER_HAVE_LOBBY_UI
#include "lobby_screen.h"
#endif
#include <algorithm>
#include <vector>

namespace {

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

static bool IsScreenState(Uint8 state) {
using namespace GameState;
switch(state){
case MAINMENU:
case LOBBYCONNECT:
case LOBBY:
case CREATECHARACTER:
case UPDATING:
case MISSIONSUMMARY:
case OPTIONS:
case OPTIONSCONTROLS:
case OPTIONSDISPLAY:
case OPTIONSAUDIO:
return true;
default:
return false;
}
}

static bool ScreenStatePlaysMenuMusic(Uint8 state) {
using namespace GameState;
switch(state){
case MAINMENU:
case LOBBYCONNECT:
case LOBBY:
case CREATECHARACTER:
case UPDATING:
case MISSIONSUMMARY:
return true;
default:
return false;
}
}
} // namespace

void GameUiPipeline::PrepareClientUiFrame(Surface& surface) {
float uiScale = game.world.map.loaded
? GameplayUiScaleForSurface(surface.w, surface.h)
: MenuUiScaleForSurface(surface.w, surface.h);
int virtualW;
int virtualH;
if(game.world.map.loaded){
virtualW = kLegacyRenderWidth;
virtualH = kLegacyRenderHeight;
}else{
virtualW = std::max(1, static_cast<int>(surface.w / uiScale));
virtualH = std::max(1, static_cast<int>(surface.h / uiScale));
}
float mx = static_cast<float>(game.world.localinput.mousex);
float my = static_cast<float>(game.world.localinput.mousey);
bool down = game.world.localinput.mousedown;
if(game.gameRenderer.GetWindow()){
Uint32 buttons = SDL_GetMouseState(&mx, &my);
down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
int windowW = 0;
int windowH = 0;
SDL_GetWindowSize(game.gameRenderer.GetWindow(), &windowW, &windowH);
float pixelX = mx;
float pixelY = my;
if(windowW > 0 && windowH > 0 && surface.w > 0 && surface.h > 0){
pixelX = (mx / static_cast<float>(windowW)) * static_cast<float>(surface.w);
pixelY = (my / static_cast<float>(windowH)) * static_cast<float>(surface.h);
}
int offsetX = 0;
int offsetY = 0;
CenteredLayoutOffset(surface.w, surface.h, virtualW, virtualH,
                     uiScale, offsetX, offsetY);
clientUiInput.SetPolledSurfacePointer(
(pixelX - static_cast<float>(offsetX)) / static_cast<float>(uiScale),
(pixelY - static_cast<float>(offsetY)) / static_cast<float>(uiScale),
down);
}else{
clientUiInput.SetPolledSurfacePointer(mx, my, down);
}
float deltaTimeSeconds =
static_cast<float>(GASLoader::Get().gameengine.tickIntervalMs) / 1000.0f;
preparedUiInput = clientUiInput.BuildFrame(virtualW, virtualH, uiScale, deltaTimeSeconds);
Uint64 now = SDL_GetTicks();
float animationDeltaSeconds = 0.0f;
if(lastUiAnimationMs != 0 && now >= lastUiAnimationMs){
animationDeltaSeconds = static_cast<float>(now - lastUiAnimationMs) / 1000.0f;
if(animationDeltaSeconds > 0.25f) animationDeltaSeconds = 0.25f;
}
lastUiAnimationMs = now;
preparedUiInput.animationDeltaSeconds = animationDeltaSeconds;
preparedUiInput.animationStepSeconds = game.gameRenderer.LegacyUiAnimationStepSeconds();
hasPreparedUiInput = true;
}

void GameUiPipeline::BeginPreparedClientUiFrame() {
if(!hasPreparedUiInput) {
PrepareClientUiFrame(game.GetScreenBuffer());
}
clientUi.BeginFrame(preparedUiInput);
}

void GameUiPipeline::EndClientUiFrame() {
clientUi.EndFrame();
hasPreparedUiInput = false;
}

void GameUiPipeline::BuildVisibleClientUi() {
silencer::client_ui::HudView hudView;
const bool hasHud = game.world.map.loaded;
if(game.world.map.loaded){
hudView = silencer::client_ui::BuildHudView(game.world);
}
clientUi.BuildRetainedUi(
game.screenContext,
hasHud ? &hudView : nullptr,
hasHud ? &game.world.resources : nullptr,
game.renderer.GetHudAnimationPhase());
}

void GameUiPipeline::DrawInGameWorldInsets(Surface& surface, float frametime) {
Player * localplayer = game.world.GetPeerPlayer(game.world.GetLocalPeerId());
if(!localplayer) return;
Renderer::Rect dstrect;
for(int slot = 0; slot < 2; ++slot){
if(!game.world.IsSystemCameraActive(slot)) continue;
Surface systemscreen(135, 44, 1);
Camera camera(135 * 2, 44 * 2);
Object * followobject = game.world.GetObjectFromId(game.world.GetSystemCameraFollowId(slot));
int px = 0;
int py = 0;
if(followobject){
px = followobject->x + ((followobject->oldx - followobject->x) * frametime);
py = followobject->y + ((followobject->oldy - followobject->y) * frametime);
if(slot == 1 && followobject->type == ObjectTypes::DETONATOR){
Detonator * detonator = static_cast<Detonator*>(followobject);
if(detonator->HasDetonated() && py < detonator->lowestypos){
py = detonator->lowestypos;
}
}
}
camera.Follow(game.world,
              px + game.world.GetSystemCameraX(slot),
              py + game.world.GetSystemCameraY(slot),
              0, 0, 0, 0);
game.renderer.DrawWorldScaled(&systemscreen, camera, 3, frametime);
game.renderer.EffectRampColor(&systemscreen, 0, 190);
dstrect.x = (slot == 0) ? 5 : 500;
dstrect.y = (slot == 0) ? 349 : 348;
Renderer::BlitSurface(&systemscreen, 0, &surface, &dstrect);
}
dstrect.x = 235;
dstrect.y = 419;
Renderer::BlitSurface(&game.world.map.minimap.surface, 0, &surface, &dstrect);
}

void GameUiPipeline::RenderClientUiFrame(Surface& surface, float frametime) {
if(!clientUi.HasScreens() && !game.world.map.loaded){
return;
}

PrepareClientUiFrame(surface);
BeginPreparedClientUiFrame();
BuildVisibleClientUi();
EndClientUiFrame();
clientUi.RenderRetainedScreens(game.renderer, game.world.resources, surface);
if(game.state != GameState::FADEOUT){
std::vector<silencer::ui::UiAction> unhandledUiActions =
clientUi.DispatchInput(game.screenContext, preparedUiInput);
if(!clientUi.HasScreens() && game.world.map.loaded){
silencer::client_ui::InGameUiActionResult result =
inGameUi.ApplyActions(game.world.peers.localpeerid, unhandledUiActions);
if(result.focusChatInput){
clientUi.FocusInteractableById("ingame.chat");
}
}
bool nowFocused = clientUi.HasTextInputFocus();
if(nowFocused && !textInputFocused){
SDL_StartTextInput(game.gameRenderer.GetWindow());
}else if(!nowFocused && textInputFocused){
SDL_StopTextInput(game.gameRenderer.GetWindow());
}
textInputFocused = nowFocused;
}
}

void GameUiPipeline::ResetUiFrameDeltas() {
clientUiInput.EndFrame();
preparedUiInput.pointer.wheelX = 0.0f;
preparedUiInput.pointer.wheelY = 0.0f;
preparedUiInput.textInput.clear();
preparedUiInput.navActions.clear();
preparedUiInput.bindingInputs.clear();
preparedUiInput.controlCommands.clear();
}
GameUiPipeline::GameUiPipeline(Game & g)
: game(g), clientUi(), inGameUi(g.world), hasPreparedUiInput(false),
  lastUiAnimationMs(0), textInputFocused(false) {
}

bool GameUiPipeline::HasInputTarget() {
if(Top()) return true;
return inGameUi.HasInputTarget(game.world.peers.localpeerid);
}

bool GameUiPipeline::TickScreenState(Uint8 state, bool entering) {
if(!IsScreenState(state)) return false;
if(entering){
EnterScreenState(state);
}else if(ScreenStatePlaysMenuMusic(state)){
PlayMenuMusicIfReady();
}
return true;
}

void GameUiPipeline::EnterScreenState(Uint8 state) {
using namespace GameState;
switch(state){
case MAINMENU:
game.world.Disconnect();
game.world.gameplaystate = World::NONE;
game.world.lobby.Disconnect();
game.gameSession.UnloadGame();
game.world.GetAuthorityPeer()->controlledlist.clear();
game.world.DestroyAllObjects();
break;
case LOBBYCONNECT:
game.world.GetAuthorityPeer()->controlledlist.clear();
game.world.DestroyAllObjects();
game.world.lobby.ClearGames();
game.world.lobby.state = Lobby::WAITING;
break;
case LOBBY:
game.world.lobby.ForgetAllUserInfo();
game.world.gameplaystate = World::INLOBBY;
game.gameSession.UnloadGame();
game.world.Disconnect();
game.world.choosingtech = false;
game.world.lobby.channelchanged = true;
break;
case CREATECHARACTER:
case UPDATING:
game.world.GetAuthorityPeer()->controlledlist.clear();
game.world.DestroyAllObjects();
break;
case MISSIONSUMMARY:
game.gameSession.UnloadGame();
game.world.Disconnect();
break;
case OPTIONS:
case OPTIONSCONTROLS:
case OPTIONSDISPLAY:
case OPTIONSAUDIO:
game.world.DestroyAllObjects();
break;
default:
return;
}
ShowScreenForState(state);
}

void GameUiPipeline::PlayMenuMusicIfReady() {
if(game.gameSession.AmbienceMixerRef().FadedIn()){
game.gameSession.AmbienceMixerRef().PlayMusic(game.world.resources.menumusic);
}
}

bool GameUiPipeline::ShowScreenForState(Uint8 state) {
using namespace GameState;
switch(state){
case MAINMENU:
Push(std::make_unique<MainMenuScreen>());
return true;
case LOBBYCONNECT:
Push(std::make_unique<LobbyConnectScreen>());
return true;
case LOBBY:
#ifdef SILENCER_HAVE_LOBBY_UI
Push(std::make_unique<LobbyScreen>());
return true;
#else
return false;
#endif
case CREATECHARACTER:
Push(std::make_unique<CharacterCreateScreen>());
return true;
case UPDATING:
Push(std::make_unique<UpdateScreen>());
return true;
case MISSIONSUMMARY:
Push(std::make_unique<MissionSummaryScreen>());
return true;
case OPTIONS:
Push(std::make_unique<OptionsScreen>());
return true;
case OPTIONSCONTROLS:
Push(std::make_unique<OptionsControlsScreen>());
return true;
case OPTIONSDISPLAY:
Push(std::make_unique<OptionsDisplayScreen>());
return true;
case OPTIONSAUDIO:
Push(std::make_unique<OptionsAudioScreen>());
return true;
default:
return false;
}
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
