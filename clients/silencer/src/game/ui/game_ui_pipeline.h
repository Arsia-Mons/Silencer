#ifndef GAME_UI_PIPELINE_H
#define GAME_UI_PIPELINE_H

#include "client/ui/ClayBridgeFrameBackend.h"
#include "client/ui/ClientUi.h"
#include "client/ui/ClientUiInput.h"
#include "client/ui/ingame/InGameUiController.h"
#include "clay/clay.h"
#include "keybinds.h"
#include "surface.h"
#include "ui/runtime/UiInputState.h"
#include <cstdint>
#include <memory>

class Game;
class Screen;

namespace silencer::cppx_ui {
class PipelineHost;
}
namespace client::ui {
enum class SessionPhase;
}

class GameUiPipeline
{
public:
explicit GameUiPipeline(Game & game);
~GameUiPipeline();

void PrepareClientUiFrame(Surface & surface);
void BeginPreparedClientUiFrame();
Clay_RenderCommandArray EndClientUiFrame();
void BuildVisibleClientUi(Surface & surface, float frametime);
void DrawInGameWorldInsets(Surface & surface, float frametime);
void RenderClientUiFrame(Surface & surface, float frametime);
void ResetUiFrameDeltas();
bool HasInputTarget();
void Push(std::unique_ptr<Screen> s);
void Pop();
void Replace(std::unique_ptr<Screen> s);
Screen * Top() const;
void QueueKeyboardInputForScancode(int scancode, const Uint8 * keystate,
const KeyMap & keymap, const GamepadState & gamepadstate);

silencer::client_ui::ClientUi & ClientUiRef() { return clientUi; }
const silencer::client_ui::ClientUi & ClientUiRef() const { return clientUi; }
silencer::client_ui::ClientUiInput & UiInput() { return clientUiInput; }
const silencer::client_ui::ClientUiInput & UiInput() const { return clientUiInput; }
const silencer::ui::UiInputState & CurrentUiInput() const { return preparedUiInput; }
silencer::ui::UiInteractionRegistry & UiInteractions() { return clientUi.Interactions(); }
const silencer::ui::UiInteractionRegistry & UiInteractions() const { return clientUi.Interactions(); }
silencer::client_ui::InGameUiController & InGameUi() { return inGameUiController; }

// --- SIL-14: golden retained cppx render path (the live UI) --------------
// RenderClientUiFrame drives the golden client::ui::UiPipeline (AppRoot + the
// session-phase reconciler) through the SIL-11 PipelineHost, producing a
// premultiplied RGBA frame that GameRenderer::Present hands to
// RenderDevice::UploadUiFrame. The Clay frame path is retired; the Clay runtime
// objects themselves are removed in SIL-22.
// The cppx RGBA produced this frame (w*h*4, premultiplied), or null when the
// cppx path did not render this frame. Owned by the PipelineHost; valid until
// the next RenderClientUiFrame.
const uint8_t * CppxUiFrame(int & outW, int & outH) const;

private:
void RenderCppxClientUiFrame(Surface & surface);
client::ui::SessionPhase CurrentSessionPhase() const;

Game & game;
silencer::client_ui::ClayBridgeFrameBackend uiClayBackend;
silencer::ui::ClayService uiClayService;
silencer::client_ui::ClientUi clientUi;
silencer::client_ui::ClientUiInput clientUiInput;
silencer::client_ui::InGameUiController inGameUiController;
silencer::ui::UiInputState preparedUiInput;
bool hasPreparedUiInput;
Uint64 lastUiAnimationMs;
bool textInputFocused;

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
};

#endif
