#include "ui/game_ui_pipeline.h"

#include "game.h"
#include "world.h"
#include "resources/resources.h" // spritebank (SIL-87 chrome bake)
#include "lobby.h"
#include "lobbygame.h"
#include "peer.h"
#include "platform/os.h"
#include "updater.h"
#include "camera.h"
#include "detonator.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "buyableitem.h"
// SIL-14 golden cppx render path.
#include "render/cppx_ui/pipeline_host.h"
#include "client/ui/app_shell/app_root.h"
#include "client/ui/app_shell/client_ui.h"
#include "client/ui/app_theme.h"
#include "client/ui/screens/gallery.h"
#include "client/ui/screens/message_modal.h"
#include "client/ui/screens/password_modal.h"
#include "client/ui/hooks/use_key_map.h"
#include "client/ui/hooks/use_keybind_capture.h"
#include "client/ui/hooks/use_session.h"
#include "client/ui/hooks/use_settings.h"
#include "client/ui/hooks/use_updater.h"
#include "client/ui/providers/app_provider.h"
#include "client/ui/providers/chrome_textures_provider.h"
#include "client/ui/providers/key_map_provider.h"
#include "client/ui/providers/keybind_capture_provider.h"
#include "client/ui/providers/lobby_provider.h"
#include "client/ui/providers/server_provider.h"
#include "client/ui/providers/session_provider.h"
#include "client/ui/providers/settings_provider.h"
#include "client/ui/providers/updater_provider.h"
#include "client/ui/providers/world_session_provider.h"
#include "ui/lobby_ui_model.h"
#include "ui/world_session_model.h"
#include "player.h"
#include "actor/user.h"
#include "session_phase.h"
#include "config.h"
#include "audio.h"
#include "renderdevice.h"
#include "ui/runtime/react.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
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

// Label a BindingKey for the UI (the UI never re-derives labels): keyboard via
// GetKeyName, mouse to LMB/MMB/RMB, gamepad button/axis via GamepadShortLabel
// (type-aware). Shared by the use_key_map read view + the capture pending chord.
client::ui::KeyMapChip ChipFromBindingKey(const BindingKey &bk,
                                          SDL_GamepadType padType) {
  client::ui::KeyMapChip chip;
  chip.device = bk.device;
  chip.code = bk.code;
  chip.axis_dir = bk.axisDir;
  switch (bk.device) {
  case BindingDevice::Keyboard:
    chip.label = KeyMap::GetKeyName((SDL_Scancode)bk.code);
    break;
  case BindingDevice::Mouse:
    chip.label = (bk.code == 1)   ? "LMB"
                 : (bk.code == 2) ? "MMB"
                 : (bk.code == 3) ? "RMB"
                                  : ("M" + std::to_string(bk.code));
    break;
  default: { // GamepadButton / GamepadAxis
    std::string s = Stringify(bk);
    auto colon = s.find(':');
    std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
    chip.label = GamepadShortLabel(raw, padType);
    break;
  }
  }
  return chip;
}
} // namespace

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

GameUiPipeline::GameUiPipeline(Game & g) : game(g) {}

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

void GameUiPipeline::BakeChromeTextures() {
// Bake the curated legacy chrome sprites into texture_ids. Runs once per host
// renderer lifetime (guarded by PipelineHost::chrome_needs_bake). This is the
// ONLY place that reads the indexed spritebank + active palette; the resulting
// opaque ids travel to screens via the ChromeTexturesProvider.
cppxChrome = {};
if(!cppxHost) return;
// Use the BASE palette, not GetPaletteColors(): the latter is the display cache
// that is faded to black on menu/transition screens, which would bake the chrome
// sprites pure black. The base palette carries the authored sprite colors (the
// green oval lives at indices 210/213-224).
const SDL_Color *palette = game.renderer.palette.GetColors();
if(!palette) return;
const auto &banks = game.world.resources.spritebank;

auto bake = [&](size_t bank, size_t index, uint32_t &id_out){
if(bank >= banks.size() || index >= banks[bank].size()) return;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) return;
uint32_t id = cppxHost->bake_chrome_sprite(sp->pixels.data(), sp->w, sp->h,
                                           palette);
if(id) id_out = id;
};

// bank 6 — the green oval menu button, per legacy size (idx7 Md / idx28 Sm /
// idx23 Lg).
bake(6, 7, cppxChrome.oval_md);
bake(6, 28, cppxChrome.oval_sm);
bake(6, 23, cppxChrome.oval_lg);
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

// SIL-87: bake the curated legacy chrome sprites into texture_ids once per
// renderer lifetime (re-baked after a resize reset, never per frame). The
// composition root is the only place that may read the indexed spritebank +
// palette; the ids flow to screens through the ChromeTexturesProvider below.
if(cppxHost->chrome_needs_bake()){
BakeChromeTextures();
cppxHost->mark_chrome_baked();
}

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
combo.chips.push_back(ChipFromBindingKey(bk, padType));
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

// Keybind capture model (doc §7b): the comp-root-owned capture state, with the
// pending chord labeled, + the begin/cancel/confirm closures. Published
// globally (the raw multi-device edge source is global) though only
// OptionsControls consumes it.
client::ui::KeybindCapture capture = {};
{
SDL_Gamepad * pad = game.GetGamepad();
SDL_GamepadType padType = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
capture.capturing = keybindCapture_.active;
capture.target_action = keybindCapture_.targetAction;
capture.target_action_label = GetActionInfo(keybindCapture_.targetAction).label;
capture.target_combo_index = keybindCapture_.targetComboIndex;
for(const BindingKey & bk : keybindCapture_.pending){
capture.pending_chips.push_back(ChipFromBindingKey(bk, padType));
}
capture.begin_capture = [this](Action a, int idx){ BeginKeybindCapture(a, idx); };
capture.cancel_capture = [this](){ CancelKeybindCapture(); };
capture.confirm_chord = [this](){ ConfirmKeybindChord(); };
}

// Lobby model (doc §5/§6): the per-tick snapshot — captured under LockMutex
// before this build, so no build-time lock — plus the queued intents over the
// public lobby seam. Consumed by LobbyConnect (connect/auth) and MissionSummary
// (progression). The connect *orchestration* lives on the game tick
// (LobbyConnectFlow); this carries only the credential-submit/cancel + upgrade/
// finish intents. SIL-20.
client::ui::LobbyProviderValue lobby = {};
lobby.snapshot = lobbySnapshot_;
lobby.connect = [this](const std::string & user, const std::string & pass){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, user, pass](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
if(lb.state == Lobby::AUTHENTICATING){
lb.SetLocalUsername(user.c_str());
lb.SendCredentials(user.c_str(), pass.c_str());
lb.state = Lobby::AUTHSENT;
}
lb.UnlockMutex();
});
};
lobby.cancel = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.Disconnect();
lb.UnlockMutex();
game.GoToState(GameState::MAINMENU);
});
};
lobby.upgrade = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
if(index < 0 || index >= 6) return;
static const Lobby::StatID kIds[6] = {Lobby::STAT_ENDURANCE, Lobby::STAT_SHIELD,
Lobby::STAT_JETPACK, Lobby::STAT_TECHSLOTS, Lobby::STAT_HACKING, Lobby::STAT_CONTACTS};
Lobby & lb = game.world.lobby;
lb.LockMutex();
User * user = lb.GetUserInfo(lb.accountid);
if(user) lb.UpgradeStat(user->selectedcharid, user->statsagency, kIds[index]);
lb.UnlockMutex();
});
};
lobby.finish = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
bool authed = (lb.state == Lobby::AUTHENTICATED);
if(authed) lb.JoinChannel(lb.lastchannel);
lb.UnlockMutex();
game.GoToState(authed ? GameState::LOBBY : GameState::MAINMENU);
});
};
lobby.create_character = [this](const std::string & alias, int agency){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, alias, agency](){
if(agency < 0 || agency > 4) return;
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.charactersreceived = false;
lb.CreateCharacter(alias.c_str(), (Uint8)agency);
lb.UnlockMutex();
// Routing waits for the roster to grow (CREATECHARACTER tick reconcile).
});
};
lobby.select_character = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
Lobby & lb = game.world.lobby;
bool selected = false;
lb.LockMutex();
if(index >= 0 && index < (int)lb.characters.size()){
lb.SelectCharacter(lb.characters[index].id);
selected = true;
}
lb.UnlockMutex();
if(selected) game.GoToState(GameState::LOBBY);
});
};
lobby.send_chat = [this](const std::string & message){
if(message.empty()) return;
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, message](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.SendChat(lb.channel, message.c_str());
lb.UnlockMutex();
});
};
// Games browser intents (doc §6): id-based join/spectate/create over the public
// seam, queued. The LOBBY-tick game-join pump drives connect → staging and the
// create finalization (auto-join on creategamestatus==1 && creategameclicked).
lobby.join_game = [this](uint32_t id, const std::string & password){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, id, password](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
LobbyGame * lg = lb.GetGameById(id);
lb.UnlockMutex();
if(!lg) return;
game.currentlobbygameid = id;
char * pw = password.empty() ? nullptr : const_cast<char*>(password.c_str());
game.JoinGame(*lg, pw);
});
};
lobby.spectate_game = [this](uint32_t id){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, id](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
LobbyGame * lg = lb.GetGameById(id);
lb.UnlockMutex();
if(!lg) return;
game.currentlobbygameid = id;
game.SpectateGame(*lg, nullptr);
});
};
lobby.create_game = [this](const client::ui::CreateGameRequest & req){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, req](){
if(req.map.empty()) return;
MapDownloader & md = game.gameSession.MapDownloaderRef();
std::string path = md.FindMap(req.map.c_str());
if(path.empty()) return;
unsigned char maphash[20] = {0};
md.CalculateMapHash(path.c_str(), &maphash);
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.CreateGame(req.name.c_str(), req.map.c_str(), maphash,
req.password.empty() ? nullptr : req.password.c_str(),
req.security, req.min_level, req.max_level,
req.max_players, req.max_teams, req.spectatable);
lb.UnlockMutex();
// The pump auto-joins our own created game once the lobby confirms it.
game.creategameclicked = true;
});
};
// Staging room intents (doc §6/§7a): ready/change-team/leave over the public
// World seam. Host-ready is guarded hook-side (ishost && !AllPeersDownloadedMap).
lobby.send_ready = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Peer * lp = game.world.GetPeer(game.world.GetLocalPeerId());
bool host = lp && lp->ishost;
if(!host || game.world.AllPeersDownloadedMap()) game.world.SendReady();
});
};
lobby.change_team = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
game.world.ChangeTeam();
});
};
lobby.leave_game = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
game.LeaveJoinedGame();
});
};

// In-match model (doc §5/§6): the per-tick world-session snapshot + the queued
// intents over the public Game/World seam. Consumed by InGameScreen
// (use_player_status / use_match). Snapshot is empty outside the in-match phases.
client::ui::WorldSessionValue world_session = {};
world_session.snapshot = worldSessionSnapshot_;
world_session.select_inventory_slot = [this](int slot){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, slot](){
if(slot < 0 || slot > 3) return;
Player * p = game.world.GetPeerPlayer(game.world.GetLocalPeerId());
if(p) p->currentinventoryitem = (Uint8)slot;
});
};
world_session.confirm_quit = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
// Quit to the menu (the full pause/leave→summary flow lands with PauseScreen).
game.GoToState(GameState::MAINMENU);
});
};
// SIL-21 (5/n) in-match overlay intents (doc §6/§7a): buy/tech station, chat
// compose, and the scoreboard toggle — queued over the public World/Player seam
// (drained after render, FADEOUT-gated). The viewed agent matches the snapshot.
world_session.buytech_select = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(!p || !(p->isbuying || p->techstationactive)) return;
std::vector<BuyableItem *> items;
p->CollectBuyMenuItems(game.world, p->techstationactive, items);
if(items.empty()) return;
int i = index < 0 ? 0 : (index >= (int)items.size() ? (int)items.size() - 1 : index);
if(p->techstationactive) p->techifacelastitem = i; else p->buyifacelastitem = i;
});
};
world_session.buytech_purchase = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(!p || !(p->isbuying || p->techstationactive)) return;
std::vector<BuyableItem *> items;
p->CollectBuyMenuItems(game.world, p->techstationactive, items);
if(items.empty()) return;
int i = index < 0 ? 0 : (index >= (int)items.size() ? (int)items.size() - 1 : index);
Uint8 id = items[i]->id;
if(p->isbuying) p->BuyItem(game.world, id);
else if(p->InOwnBase(game.world)) p->RepairItem(game.world, id);
else p->VirusItem(game.world, id);
});
};
world_session.buytech_close = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(p){ p->isbuying = false; p->techstationactive = false; }
});
};
world_session.chat_send = [this](const std::string & text){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, text](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(!p) return;
if(!text.empty()){
char buf[101];
std::strncpy(buf, text.c_str(), sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';
game.world.SendChat(p->chatwithteam, buf);
}
p->chatActive = false;
p->chatText[0] = '\0';
});
};
world_session.chat_cancel = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(p){ p->chatActive = false; p->chatText[0] = '\0'; }
});
};
world_session.chat_toggle_channel = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(p) p->chatwithteam = !p->chatwithteam;
});
};
world_session.set_show_player_list = [this](bool show){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, show](){
game.world.SetShowingPlayerList(show);
});
};
world_session.request_peer_list = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
game.world.RequestPeerList();
});
};

// Global FrameProvider chain (doc §5), outermost (Theme) → innermost
// (WorldSession): Theme ▸ Server ▸ App ▸ Session ▸ Settings ▸ KeyMap ▸ Updater ▸
// KeybindCapture ▸ Lobby ▸ WorldSession ▸ <screen>.
::ui::UiElement tree = client::ui::WorldSessionProvider(
client::ui::WorldSessionValue{std::move(world_session)}, ::ui::children({child}));
tree = client::ui::LobbyProvider(
client::ui::LobbyProviderValue{std::move(lobby)}, ::ui::children({tree}));
tree = client::ui::KeybindCaptureProvider(
client::ui::KeybindCaptureProviderValue{std::move(capture)}, ::ui::children({tree}));
tree = client::ui::UpdaterProvider(
client::ui::UpdaterProviderValue{updater}, ::ui::children({tree}));
tree = client::ui::KeyMapProvider(
client::ui::KeyMapProviderValue{std::move(key_map)}, ::ui::children({tree}));
tree = client::ui::SettingsProvider(
client::ui::SettingsProviderValue{settings}, ::ui::children({tree}));
tree = client::ui::SessionProvider(
client::ui::SessionProviderValue{session}, ::ui::children({tree}));
tree = client::ui::AppProvider(
client::ui::AppProviderValue{[this]{ game.quitRequested = true; }},
::ui::children({tree}));
// SIL-87: baked legacy-sprite chrome ids (read by use_chrome()).
tree = client::ui::ChromeTexturesProvider(cppxChrome, ::ui::children({tree}));
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

// SIL-18 input: the accumulated per-frame edges (events.cpp windowed +
// control-socket injection) plus the derived pointer. An injected click is a
// single-frame press+release at a UI-space point (so the control socket can
// activate a node by location); otherwise the real mouse drives hover/press.
client::ui::UiPipelineFrame frame = {};
frame.layout = {static_cast<float>(rw), static_cast<float>(rh)};
frame.input = uiInput_;
float mx = -1000.0f;
float my = -1000.0f;
if(injectedPointer_){
mx = injectedPointerX_;
my = injectedPointerY_;
frame.input.pointer_pressed = true;
frame.input.pointer_released = true;
frame.input.pointer_down = false;
frame.input.source = ::ui::UiFocusSource::Mouse;
}else if(hasInjectedHover_){
// Sticky headless hover: park the pointer at the injected point (no press) so
// focus-follows-hover tracks it. Persists until the next InjectPointerMove.
mx = injectedHoverX_;
my = injectedHoverY_;
frame.input.source = ::ui::UiFocusSource::Mouse;
}else if(win){
float wx = 0.0f;
float wy = 0.0f;
Uint32 buttons = SDL_GetMouseState(&wx, &wy);
int ww = 0;
int wh = 0;
SDL_GetWindowSize(win, &ww, &wh);
mx = (ww > 0) ? (wx / static_cast<float>(ww)) * static_cast<float>(rw) : wx;
my = (wh > 0) ? (wy / static_cast<float>(wh)) * static_cast<float>(rh) : wy;
bool down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
frame.input.pointer_down = down;
frame.input.pointer_pressed = down && !prevPointerDown_;
frame.input.pointer_released = !down && prevPointerDown_;
if(down || frame.input.pointer_pressed || frame.input.pointer_released){
frame.input.source = ::ui::UiFocusSource::Mouse;
}
prevPointerDown_ = down;
}
frame.pointer = {mx, my};

// SIL-20: capture the lobby read-state once per tick, under LockMutex, *before*
// the build below reads it (the frame provider copies lobbySnapshot_ with no
// build-time lock). Default/empty outside lobby phases.
lobbySnapshot_ = silencer::game_ui::CaptureLobbySnapshot(game, CurrentSessionPhase());
// SIL-21 (3/n): fold in the bundled-map choices for the GameCreatePanel. Maps
// don't change at runtime, so list them once (disk read on the game thread).
if(!bundledMapsListed_){
bundledMaps_ = game.gameSession.MapDownloaderRef().ListFiles((GetResDir() + "level").c_str());
bundledMapsListed_ = true;
}
lobbySnapshot_.bundled_maps = bundledMaps_;
// SIL-21 (4/n): capture the in-match read-state (viewed agent + replicated match
// state) on the game thread before the build. Empty outside the match phases.
worldSessionSnapshot_ = silencer::game_ui::CaptureWorldSessionSnapshot(game, CurrentSessionPhase());

int ow = 0;
int oh = 0;
const uint8_t * rgba = cppxHost->render(frame, &ow, &oh);
if(rgba){
cppxUiRgba = rgba;
cppxUiW = ow;
cppxUiH = oh;
}

// Text-input platform gating (windowed only): the cppx pipeline reports whether
// the focused node is a text field; toggle SDL text input to match. This is the
// only owner of SDL_StartTextInput/StopTextInput (see src/game/CLAUDE.md).
if(win){
bool wants = cppxHost->pipeline().client_ui().wants_text_input();
if(wants != textInputActive_){
if(wants) SDL_StartTextInput(win); else SDL_StopTextInput(win);
textInputActive_ = wants;
}
}

// One-frame edges are consumed; reset for the next accumulation window.
uiInput_ = {};
injectedPointer_ = false;
#else
(void)surface;
#endif
}

void GameUiPipeline::InjectPointerClick(float x, float y) {
injectedPointer_ = true;
injectedPointerX_ = x;
injectedPointerY_ = y;
}

void GameUiPipeline::InjectPointerMove(float x, float y) {
hasInjectedHover_ = true;
injectedHoverX_ = x;
injectedHoverY_ = y;
}

client::ui::ClientUi * GameUiPipeline::TryClientUi() {
return cppxHost ? &cppxHost->pipeline().client_ui() : nullptr;
}

void GameUiPipeline::ShowPasswordModal(const char * title) {
client::ui::ClientUi * ui = TryClientUi();
if(!ui) return;
passwordModal_ = {true, false, {}};
ui->push_screen(std::make_unique<client::ui::PasswordModalScreen>(
title ? title : "Password",
[this](const std::string & value){
passwordModal_.submitted = true;
passwordModal_.value = value;
}));
}

void GameUiPipeline::ShowMessageModal(const char * title, const char * message) {
client::ui::ClientUi * ui = TryClientUi();
if(!ui) return;
ui->push_screen(std::make_unique<client::ui::MessageModalScreen>(
title ? title : "", message ? message : ""));
}

void GameUiPipeline::ShowGallery() {
client::ui::ClientUi * ui = TryClientUi();
if(!ui) return;
ui->push_screen(std::make_unique<client::ui::GalleryScreen>());
}

void GameUiPipeline::BeginKeybindCapture(Action action, int comboIndex) {
keybindCapture_.active = true;
keybindCapture_.targetAction = action;
keybindCapture_.targetComboIndex = comboIndex;
keybindCapture_.pending.clear();
}

bool GameUiPipeline::FeedKeybindEdge(const BindingKey & key) {
if(!keybindCapture_.active) return false;
if((int)keybindCapture_.pending.size() >= CHORD_CAP) return false;
for(const BindingKey & k : keybindCapture_.pending){
if(k.device == key.device && k.code == key.code && k.axisDir == key.axisDir){
return false; // dedup: a held edge only adds its chip once
}
}
keybindCapture_.pending.push_back(key);
return true;
}

void GameUiPipeline::CancelKeybindCapture() {
keybindCapture_.active = false;
keybindCapture_.targetComboIndex = -1;
keybindCapture_.pending.clear();
}

void GameUiPipeline::ConfirmKeybindChord() {
if(!keybindCapture_.active || keybindCapture_.pending.empty()){
CancelKeybindCapture();
return;
}
// Apply directly (not via the after-render mutation queue): this also runs
// from the control socket, which fires before begin_frame clears that queue.
// A keymap edit is local config — not a structural tree mutation or a wire
// message — so a direct, immediate write is safe and FADEOUT-gating-exempt.
if((int)keybindCapture_.pending.size() <= CHORD_CAP){
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(keybindCapture_.targetAction);
Binding b;
b.keys = keybindCapture_.pending;
int idx = keybindCapture_.targetComboIndex;
if(idx >= 0 && idx < (int)ab.bindings.size()){
ab.bindings[idx] = std::move(b);
keymapDirty_ = true;
}else if((int)ab.bindings.size() < COMBO_CAP){
ab.bindings.push_back(std::move(b));
keymapDirty_ = true;
}
}
CancelKeybindCapture();
}

