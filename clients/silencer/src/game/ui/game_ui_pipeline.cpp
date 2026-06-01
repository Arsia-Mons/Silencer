#include "ui/game_ui_pipeline.h"

#include "client/ui/screens/screen.h"
#include "game.h"
#include "lobby.h"
#include "updater.h"
#include "camera.h"
#include "detonator.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "client/ui/hud/InGameHud.h"
#include "client/ui/hud/InGameOverlays.h"
#include "client/ui/views/HudView.h"
#include "clay_ui_compositor.h"
// SIL-14 golden cppx render path.
#include "render/cppx_ui/pipeline_host.h"
#include "client/ui/app_shell/app_root.h"
#include "client/ui/app_theme.h"
#include "client/ui/hooks/use_key_map.h"
#include "client/ui/hooks/use_session.h"
#include "client/ui/hooks/use_settings.h"
#include "client/ui/hooks/use_updater.h"
#include "client/ui/providers/app_provider.h"
#include "client/ui/providers/key_map_provider.h"
#include "client/ui/providers/server_provider.h"
#include "client/ui/providers/session_provider.h"
#include "client/ui/providers/settings_provider.h"
#include "client/ui/providers/updater_provider.h"
#include "session_phase.h"
#include "config.h"
#include "audio.h"
#include "renderdevice.h"
#include "ui/runtime/react.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {
// SIL-15 use_key_map: convert UI chips back to an input Binding, enforcing the
// chord cap (reject, never truncate). Returns false if the chord is empty or
// over CHORD_CAP.
bool ChipsToBinding(const std::vector<client::ui::KeyMapChip> &chips,
                    Binding &out) {
  if (chips.empty() || (int)chips.size() > CHORD_CAP)
    return false;
  for (const client::ui::KeyMapChip &c : chips) {
    BindingKey bk;
    bk.device = c.device;
    bk.code = c.code;
    bk.axisDir = c.axis_dir;
    out.keys.push_back(bk);
  }
  return true;
}

bool KeybindProfileIsBuiltin(const std::string &name) {
  return name == "default" || name == "wasd" || name == "gamepad";
}
} // namespace

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
silencer::clay_bridge::SetTextMeasureResources(&game.world.resources);
clientUi.BeginFrame(preparedUiInput);
}

Clay_RenderCommandArray GameUiPipeline::EndClientUiFrame() {
clientUi.EndFrame();
hasPreparedUiInput = false;
return uiClayBackend.Commands();
}

void GameUiPipeline::BuildVisibleClientUi(Surface& surface, float frametime) {
clientUi.BuildVisibleScreens(game.screenContext, surface, frametime);
if(game.world.map.loaded){
silencer::client_ui::HudView hudView =
silencer::client_ui::BuildHudView(game.world);
silencer::client_ui::BuildInGameHudUi(
game.renderer, game.world.resources, hudView, &surface, clientUi.Interactions());
silencer::client_ui::BuildInGameOverlaysUi(game.renderer, game.world.resources, hudView, &surface);
}
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
(void)frametime;
// SIL-14: the golden retained cppx path is the live UI. GameRenderer::Present
// uploads the RGBA we stash here. The Clay frame path is gone; the Clay
// runtime/objects themselves are retired in SIL-22.
RenderCppxClientUiFrame(surface);
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
: game(g), uiClayService(uiClayBackend), clientUi(uiClayService),
  inGameUiController(g.world), hasPreparedUiInput(false),
  lastUiAnimationMs(0), textInputFocused(false) {
}

GameUiPipeline::~GameUiPipeline() {
// Tear the cppx host down before the global hook runtime it depends on.
cppxHost.reset();
if(cppxReactInitialized){
react_shutdown();
cppxReactInitialized = false;
}
}

client::ui::SessionPhase GameUiPipeline::CurrentSessionPhase() const {
return silencer::game_ui::project_session_phase(game.state, game.nextstate);
}

const uint8_t * GameUiPipeline::CppxUiFrame(int & outW, int & outH) const {
outW = cppxUiW;
outH = cppxUiH;
return cppxUiRgba;
}

void GameUiPipeline::RenderCppxClientUiFrame(Surface& surface) {
cppxUiRgba = nullptr;
#ifdef SILENCER_CPPX_FONT_DIR
// Native window-pixel resolution so the UI composite maps 1:1 over the
// upscaled world frame (matches the SIL-11 demo). Headless / no window falls
// back to the surface size so the path still runs (UploadUiFrame is a no-op
// on devices without a UI composite pass).
int rw = surface.w;
int rh = surface.h;
SDL_Window * win = game.gameRenderer.GetWindow();
if(win){
int pw = 0;
int ph = 0;
if(SDL_GetWindowSizeInPixels(win, &pw, &ph) && pw > 0 && ph > 0){
rw = pw;
rh = ph;
}
}
if(rw < 1 || rh < 1) return;

if(!cppxReactInitialized){
react_init_runtime();
cppxReactInitialized = true;
}
if(!cppxHost){
cppxHost = std::make_unique<silencer::cppx_ui::PipelineHost>();
}
if(!cppxHost->ensure(rw, rh, SILENCER_CPPX_FONT_DIR)) return;

if(!cppxAppRootPushed){
// The global FrameProvider chain (doc §5), outermost-first. ServerProvider
// carries the live Game handle; SessionProvider publishes the projected phase
// to AppRoot's reconciler. Theme/App/Settings/KeyMap/Updater join as their
// hooks land.
cppxHost->pipeline().set_frame_provider([this](::ui::UiElement child){
// Assemble the session model fresh each frame: read projection + intent
// closures over the public Game command seam (no friend, no handle leak).
client::ui::Session session = {};
session.phase = CurrentSessionPhase();
session.authenticated = (game.world.lobby.state == Lobby::AUTHENTICATED);
session.paused = game.paused;
session.is_live_multiplayer = game.IsLiveMultiplayer();
session.current_game_id = game.currentlobbygameid;
session.play_online = [this]{ game.GoToState(GameState::LOBBYCONNECT); };
session.start_tutorial = [this]{ game.GoToState(GameState::SINGLEPLAYERGAME); };
session.open_character_create = [this]{ game.GoToState(GameState::CREATECHARACTER); };
session.leave_match = [this]{ game.LeaveJoinedGame(); };
session.leave_to_menu = [this]{ game.GoToState(GameState::MAINMENU); };
session.set_paused = [this](bool p){ game.paused = p; };

// Updater model: poll the self-updater's main-thread-safe atomics + map its
// state to the UI phase; intents route to the public ::Updater methods.
client::ui::UpdaterModel updater = {};
switch(game.updater.GetState()){
case ::Updater::IDLE: updater.phase = client::ui::UpdaterPhase::Idle; break;
case ::Updater::PROMPTING: updater.phase = client::ui::UpdaterPhase::Prompting; break;
case ::Updater::DOWNLOADING: updater.phase = client::ui::UpdaterPhase::Downloading; break;
case ::Updater::VERIFYING: updater.phase = client::ui::UpdaterPhase::Verifying; break;
case ::Updater::STAGING: updater.phase = client::ui::UpdaterPhase::Staging; break;
case ::Updater::FAILED: updater.phase = client::ui::UpdaterPhase::Failed; break;
case ::Updater::DONE: updater.phase = client::ui::UpdaterPhase::Done; break;
}
updater.progress = game.updater.GetProgress();
updater.error = game.updater.GetErrorMessage();
updater.retry_count = game.updater.GetRetryCount();
updater.download_url = game.updater.GetDownloadURL();
updater.can_retry = (game.updater.GetState() == ::Updater::FAILED);
updater.consent = [this]{ game.updater.Consent(); };
updater.cancel = [this]{ game.updater.Cancel(); };
updater.retry = [this]{ game.updater.Retry(); };
updater.open_download_page = [this]{
SDL_OpenURL(game.updater.GetDownloadURL().c_str());
};

// Settings model (doc §6): read the live Config + install live-apply preview
// closures over the public subsystems (SIL-6 LOCKED: live-apply preview). dirty
// = live Config diverges from the last-committed snapshot.
client::ui::Settings settings = {};
{
Config & cfg = Config::GetInstance();
if(!committedSettingsInit_){
committedSettings_ = {cfg.music, cfg.musicvolume, cfg.fullscreen, cfg.scalefilter};
committedSettingsInit_ = true;
}
settings.music = cfg.music;
settings.music_volume = cfg.musicvolume;
settings.fullscreen = cfg.fullscreen;
settings.smooth_scaling = cfg.scalefilter;
settings.dirty = (cfg.music != committedSettings_.music
|| cfg.musicvolume != committedSettings_.musicvolume
|| cfg.fullscreen != committedSettings_.fullscreen
|| cfg.scalefilter != committedSettings_.scalefilter);
settings.set_music = [this](bool on){
Config::GetInstance().music = on;
if(on) Audio::GetInstance().ResumeMusic(); else Audio::GetInstance().PauseMusic();
};
settings.set_music_volume = [this](uint8_t v){
Config::GetInstance().musicvolume = v;
Audio::GetInstance().SetMusicVolume(v);
};
settings.set_fullscreen = [this](bool on){
Config::GetInstance().fullscreen = on;
if(SDL_Window * w = game.gameRenderer.GetWindow()) SDL_SetWindowFullscreen(w, on);
};
settings.set_smooth_scaling = [this](bool on){
Config::GetInstance().scalefilter = on;
if(RenderDevice * d = game.gameRenderer.GetRenderDevice()) d->SetScaleFilter(on);
};
settings.commit = [this]{
Config & c = Config::GetInstance();
c.Save();
committedSettings_ = {c.music, c.musicvolume, c.fullscreen, c.scalefilter};
};
settings.revert = [this]{
Config & c = Config::GetInstance();
c.Load();
if(SDL_Window * w = game.gameRenderer.GetWindow()) SDL_SetWindowFullscreen(w, c.fullscreen);
if(RenderDevice * d = game.gameRenderer.GetRenderDevice()) d->SetScaleFilter(c.scalefilter);
Audio::GetInstance().SetMusicVolume(c.musicvolume);
if(c.music) Audio::GetInstance().ResumeMusic(); else Audio::GetInstance().PauseMusic();
committedSettings_ = {c.music, c.musicvolume, c.fullscreen, c.scalefilter};
};
}

// KeyMap model (doc §6/§7b): rows-of-combos read view (labels rendered here,
// since only the comp root has the gamepad type) + the six mutation intents.
// Binding mutations fork-if-builtin + enforce caps, all drained after render.
client::ui::KeyMap key_map = {};
{
const KeyMap & km = game.GetKeyMap();
SDL_Gamepad * pad = game.GetGamepad();
SDL_GamepadType padType = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
key_map.actions.reserve((int)Action::Count);
for(int i = 0; i < (int)Action::Count; ++i){
Action a = ACTION_TABLE[i].action;
client::ui::KeyMapAction outA;
outA.action = a;
outA.label = GetActionInfo(a).label;
const ActionBindings & ab = km.Get(a);
for(const Binding & b : ab.bindings){
client::ui::KeyMapCombo combo;
for(const BindingKey & bk : b.keys){
client::ui::KeyMapChip chip;
chip.device = bk.device;
chip.code = bk.code;
chip.axis_dir = bk.axisDir;
if(bk.device == BindingDevice::Keyboard){
chip.label = KeyMap::GetKeyName((SDL_Scancode)bk.code);
}else{
std::string s = Stringify(bk);
auto colon = s.find(':');
std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
chip.label = GamepadShortLabel(raw, padType);
}
combo.chips.push_back(std::move(chip));
}
outA.combos.push_back(std::move(combo));
}
key_map.actions.push_back(std::move(outA));
}
const std::string activeProfile = Config::GetInstance().active_keybind_profile;
key_map.preset_label = !km.label.empty() ? km.label
: (!km.name.empty() ? km.name : activeProfile);
key_map.preset_is_builtin = KeybindProfileIsBuiltin(activeProfile);
key_map.dirty = keymapDirty_;
key_map.add_combo = [this](Action a, std::vector<client::ui::KeyMapChip> chips){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a, chips = std::move(chips)]() mutable {
Binding b;
if(!ChipsToBinding(chips, b)) return;
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(a);
if((int)ab.bindings.size() >= COMBO_CAP) return;
ab.bindings.push_back(std::move(b));
keymapDirty_ = true;
});
};
key_map.replace_combo = [this](Action a, int idx, std::vector<client::ui::KeyMapChip> chips){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a, idx, chips = std::move(chips)]() mutable {
Binding b;
if(!ChipsToBinding(chips, b)) return;
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(a);
if(idx < 0 || idx >= (int)ab.bindings.size()) return;
ab.bindings[idx] = std::move(b);
keymapDirty_ = true;
});
};
key_map.remove_combo = [this](Action a, int idx){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a, idx](){
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(a);
if(idx < 0 || idx >= (int)ab.bindings.size()) return;
ab.bindings.erase(ab.bindings.begin() + idx);
keymapDirty_ = true;
});
};
key_map.clear_action = [this](Action a){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a](){
ForkActiveProfileIfBuiltin(game.GetKeyMap());
game.GetKeyMap().Get(a).bindings.clear();
keymapDirty_ = true;
});
};
key_map.cycle_preset = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
CycleKeybindPreset(game.GetKeyMap());
keymapDirty_ = false;
});
};
key_map.commit = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
const std::string active = Config::GetInstance().active_keybind_profile;
if(!KeybindProfileIsBuiltin(active)) game.GetKeyMap().SaveFile(WritableProfilePath(active));
Config::GetInstance().Save();
keymapDirty_ = false;
});
};
key_map.revert = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
LoadActiveKeymap(game.GetKeyMap());
Config::GetInstance().Load();
keymapDirty_ = false;
});
};
}

// Global FrameProvider chain (doc §5), outermost (Theme) → innermost
// (Updater): Theme ▸ Server ▸ App ▸ Session ▸ Settings ▸ KeyMap ▸ Updater ▸ <screen>.
::ui::UiElement tree = client::ui::UpdaterProvider(
client::ui::UpdaterProviderValue{updater}, ::ui::children({child}));
tree = client::ui::KeyMapProvider(
client::ui::KeyMapProviderValue{std::move(key_map)}, ::ui::children({tree}));
tree = client::ui::SettingsProvider(
client::ui::SettingsProviderValue{settings}, ::ui::children({tree}));
tree = client::ui::SessionProvider(
client::ui::SessionProviderValue{session}, ::ui::children({tree}));
tree = client::ui::AppProvider(
client::ui::AppProviderValue{[this]{ game.quitRequested = true; }},
::ui::children({tree}));
tree = silencer::game_ui::ServerProvider(
silencer::game_ui::ServerProviderValue{&game},
::ui::children({tree}));
tree = client::ui::ThemeProvider(::ui::children({tree}));
return tree;
});
cppxHost->pipeline().client_ui().push_screen(
std::make_unique<client::ui::AppRoot>());
cppxAppRootPushed = true;
}

// Minimal input for the scaffold cut: pointer only (the per-phase scaffolds
// are non-interactive). Full nav/text routing through focus_update lands with
// the real screens.
client::ui::UiPipelineFrame frame = {};
frame.layout = {static_cast<float>(rw), static_cast<float>(rh)};
float mx = -1000.0f;
float my = -1000.0f;
if(win){
float wx = 0.0f;
float wy = 0.0f;
Uint32 buttons = SDL_GetMouseState(&wx, &wy);
int ww = 0;
int wh = 0;
SDL_GetWindowSize(win, &ww, &wh);
mx = (ww > 0) ? (wx / static_cast<float>(ww)) * static_cast<float>(rw) : wx;
my = (wh > 0) ? (wy / static_cast<float>(wh)) * static_cast<float>(rh) : wy;
frame.input.pointer_down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
frame.input.source = ::ui::UiFocusSource::Mouse;
}
frame.pointer = {mx, my};

int ow = 0;
int oh = 0;
const uint8_t * rgba = cppxHost->render(frame, &ow, &oh);
if(rgba){
cppxUiRgba = rgba;
cppxUiW = ow;
cppxUiH = oh;
}
#else
(void)surface;
#endif
}

bool GameUiPipeline::HasInputTarget() {
if(Top()) return true;
return inGameUiController.HasInputTarget(game.world.peers.localpeerid);
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
