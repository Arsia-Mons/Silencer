#ifndef GAME_UI_PIPELINE_H
#define GAME_UI_PIPELINE_H

#include "surface.h"
#include <cstdint>
#include <memory>

class Game;

namespace silencer::cppx_ui {
class PipelineHost;
}
namespace client::ui {
enum class SessionPhase;
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
};

#endif
