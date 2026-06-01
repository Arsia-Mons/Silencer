#ifndef GAME_UI_PIPELINE_H
#define GAME_UI_PIPELINE_H

#include "surface.h"
#include "input/keybinds.h" // Action, BindingKey, BindingDevice
#include "ui/input.h"
#include "client/ui/providers/lobby_provider.h" // client::ui::LobbySnapshot
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Game;

namespace silencer::cppx_ui {
class PipelineHost;
}
namespace client::ui {
enum class SessionPhase;
class ClientUi;
}

// Owns the live client UI for one frame. SIL-14/15: the golden retained cppx
// path is the only UI — RenderClientUiFrame drives the golden
// client::ui::UiPipeline (AppRoot + the global provider chain + the session-
// phase reconciler) through the SIL-11 PipelineHost, producing a premultiplied
// RGBA frame GameRenderer::Present hands to RenderDevice::UploadUiFrame. The
// legacy immediate-mode UI frame path + its screen-context bag were removed;
// DrawInGameWorldInsets is the only non-cppx survivor (it draws the in-game
// minimap / system-camera insets straight to the world Surface).
class GameUiPipeline
{
public:
explicit GameUiPipeline(Game & game);
~GameUiPipeline();

void DrawInGameWorldInsets(Surface & surface, float frametime);
void RenderClientUiFrame(Surface & surface, float frametime);

// The cppx RGBA produced this frame (w*h*4, premultiplied), or null when the
// cppx path did not render this frame. Owned by the PipelineHost; valid until
// the next RenderClientUiFrame.
const uint8_t * CppxUiFrame(int & outW, int & outH) const;

// --- UI input + control-socket automation seam (SIL-18) ----------------
// The per-frame UI input frame. The single SDL-event site (events.cpp, windowed)
// and the control socket (headless automation) accumulate nav/confirm/cancel/
// key/text edges here; RenderCppxClientUiFrame consumes it, then clears the
// one-frame edges.
::ui::UiInputFrame & UiInput() { return uiInput_; }
// Inject a single-frame pointer click (press+release) at a UI-space point, so
// the control socket can activate a node by location.
void InjectPointerClick(float x, float y);
// The live retained UI tree owner, or null before the first cppx frame. The
// control socket reads it for introspection (read-only tree + focus snapshots,
// no friend/handle leak).
client::ui::ClientUi * TryClientUi();
// Push the props-only modal overlays (control socket today; screen-driven flows
// in later slices). Direct (non-queued) push: it adds to the screen stack
// immediately, surviving the next frame's begin_frame (which clears only the
// mutation queue).
void ShowPasswordModal(const char * title);
void ShowMessageModal(const char * title, const char * message);
// The latest password-modal interaction, for the control socket's
// password_modal_result op.
struct PasswordModalResult {
bool open = false;
bool submitted = false;
std::string value;
};
const PasswordModalResult & PasswordModal() const { return passwordModal_; }

// --- Keybind capture (SIL-19 §7b) --------------------------------------
// The capture state machine. Raw multi-device edges arrive in the game layer
// (events.cpp, gated on IsCapturingKeybind) and the control socket; the UI
// (use_keybind_capture) drives begin/confirm/cancel and renders the pending
// chord. Live here (not in React state) because the edge source is global.
bool IsCapturingKeybind() const { return keybindCapture_.active; }
void BeginKeybindCapture(Action action, int comboIndex);
// Append a raw edge to the pending chord (dedup; capped at CHORD_CAP). Returns
// true when the chip was added. No-op when not capturing.
bool FeedKeybindEdge(const BindingKey & key);
// Commit the pending chord as the target combo (fork-if-builtin, caps), then
// leave capture mode. No-op if the chord is empty.
void ConfirmKeybindChord();
void CancelKeybindCapture();
const std::vector<BindingKey> & KeybindCapturePending() const {
return keybindCapture_.pending;
}

private:
void RenderCppxClientUiFrame(Surface & surface);
client::ui::SessionPhase CurrentSessionPhase() const;

Game & game;

// cppx render path (SIL-14). Lazily created on first cppx frame.
std::unique_ptr<silencer::cppx_ui::PipelineHost> cppxHost;
const uint8_t * cppxUiRgba = nullptr;
int cppxUiW = 0;
int cppxUiH = 0;
bool cppxReactInitialized = false;
bool cppxAppRootPushed = false;

// SIL-15 use_settings dirty tracking: snapshot of the four persisted prefs as
// of the last commit/revert; live Config diverging from it => Settings.dirty.
struct CommittedSettings {
bool music = true;
uint8_t musicvolume = 48;
bool fullscreen = true;
bool scalefilter = true;
};
CommittedSettings committedSettings_ = {};
bool committedSettingsInit_ = false;
// SIL-15 use_key_map dirty flag (the live KeyMap has no dirty bit).
bool keymapDirty_ = false;

// SIL-20 lobby snapshot: the per-tick POD copy of lobby read-state, captured
// under LockMutex before the build phase (doc §5). The frame provider reads
// this when assembling the LobbyProvider value — no build-time lock.
client::ui::LobbySnapshot lobbySnapshot_ = {};
// SIL-21 (3/n) create-form map choices: the bundled maps the GameCreatePanel
// offers. Disk-listed once (maps don't change at runtime), then folded into the
// lobby snapshot each lobby-phase frame.
std::vector<std::string> bundledMaps_ = {};
bool bundledMapsListed_ = false;

// SIL-18 UI input: the per-frame edges (events.cpp + control socket) and the
// derived pointer state. Cleared each frame after the pipeline consumes them.
::ui::UiInputFrame uiInput_ = {};
bool injectedPointer_ = false;
float injectedPointerX_ = 0.0f;
float injectedPointerY_ = 0.0f;
bool prevPointerDown_ = false;
bool textInputActive_ = false;
PasswordModalResult passwordModal_ = {};

// SIL-19 keybind capture state. `active` gates events.cpp's raw-edge intake.
struct KeybindCaptureState {
bool active = false;
Action targetAction = Action::MoveUp;
int targetComboIndex = -1; // -1 = append a new combo
std::vector<BindingKey> pending = {}; // the chord under construction
};
KeybindCaptureState keybindCapture_ = {};
};

#endif
