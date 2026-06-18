#ifndef GAME_UI_PIPELINE_H
#define GAME_UI_PIPELINE_H

#include "surface.h"
#include "input/keybinds.h" // Action, BindingKey, BindingDevice
#include "ui/input.h"
#include "client/ui/providers/lobby_provider.h"         // client::ui::LobbySnapshot
#include "client/ui/providers/world_session_provider.h" // client::ui::WorldSessionSnapshot
#include "client/ui/hooks/use_chrome.h"                 // client::ui::ChromeTextures
#include "client/ui/hooks/use_clock.h"                  // client::ui::Clock
#include "client/ui/hooks/use_map_previews.h"           // client::ui::MapPreviews
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Game;

namespace silencer::cppx_ui {
class PipelineHost;
struct GpuUiProgram;
} // namespace silencer::cppx_ui
namespace client::ui {
enum class SessionPhase;
class ClientUi;
} // namespace client::ui

// The cppx UI composition root: drives the retained client::ui::UiPipeline through
// the PipelineHost each frame. DrawInGameWorldInsets is the one non-cppx survivor
// (it draws minimap / system-camera insets straight to the world Surface).
class GameUiPipeline {
public:
    explicit GameUiPipeline(Game &game);
    ~GameUiPipeline();

    void DrawInGameWorldInsets(Surface &surface, float frametime);
    void RenderClientUiFrame(Surface &surface, float frametime);

    // The cppx RGBA produced this frame (premultiplied), or null if it did not
    // render. Owned by the PipelineHost; valid until the next RenderClientUiFrame.
    const uint8_t *CppxUiFrame(int &outW, int &outH) const;

    // True when the IR was byte-identical to last frame (raster skipped, same
    // pixels). The caller may skip the upload ONLY if the fade alpha it applies at
    // upload time is also unchanged — fade is NOT in the IR.
    bool CppxUiUnchanged() const { return cppxUiUnchanged_; }

    // The GPU UI geometry program built this frame (windowed GPU path), or null on
    // the CPU-raster path. Owned by the PipelineHost; valid until the next frame.
    const silencer::cppx_ui::GpuUiProgram *CppxUiProgram() const { return cppxUiProgram_; }

    // Per-frame UI input. events.cpp (windowed) and the control socket accumulate
    // edges here; RenderCppxClientUiFrame consumes them.
    ::ui::UiInputFrame &UiInput() { return uiInput_; }
    // True while the focused cppx node is a text field. events.cpp reads it to keep
    // SPACE out of the confirm edge while typing (a field types a space; Enter submits).
    bool WantsTextInput() const { return textInputActive_; }
    // Inject a single-frame pointer click (press+release) at a UI-space point.
    void InjectPointerClick(float x, float y);
    // Park a sticky hover position (no press) for headless focus-follows-hover;
    // persists across frames until moved again.
    void InjectPointerMove(float x, float y);
    // Hold the pointer down across frames so the PRESSED state can be screenshotted.
    // Released by InjectPointerRelease.
    void InjectPointerPress(float x, float y);
    void InjectPointerRelease();
    // The live retained UI tree owner, or null before the first cppx frame.
    client::ui::ClientUi *TryClientUi();
    // Direct (non-queued) push: adds to the screen stack immediately, surviving the
    // next begin_frame (which clears only the mutation queue).
    void ShowPasswordModal(const char *title);
    void ShowMessageModal(const char *title, const char *message);
    // Design-system gallery overlay (visual-regression harness; not in the product UI).
    void ShowGallery();
    struct PasswordModalResult {
        bool open = false;
        bool submitted = false;
        std::string value;
    };
    const PasswordModalResult &PasswordModal() const { return passwordModal_; }

    // Keybind capture state machine. Lives here (not React state) because the raw
    // multi-device edge source is global.
    bool IsCapturingKeybind() const { return keybindCapture_.active; }
    void BeginKeybindCapture(Action action, int comboIndex);
    // Append a raw edge to the pending chord (dedup; capped at CHORD_CAP).
    bool FeedKeybindEdge(const BindingKey &key);
    void ConfirmKeybindChord();
    void CancelKeybindCapture();
    const std::vector<BindingKey> &KeybindCapturePending() const {
        return keybindCapture_.pending;
    }

    // In-game HUD per-frame sprite variants (pulse ramps + team-colorized emblems),
    // lazily baked + memoized. Returns 0 when not bakeable yet (screens tolerate).
    uint32_t EnsureHudRampVariant(uint8_t bank, uint16_t index, uint8_t rampColor,
                                  uint8_t rampPlus, uint8_t brightness = 128);
    client::ui::ChromeTextures::Sprite EnsureHudTeamEmblem(uint8_t agency,
                                                           uint8_t color);

    // UI interaction-click edges fired so far. Counted even when audio is disabled
    // (headless) so e2e can assert the edges.
    uint32_t UiClickCount() const { return uiClickCount_; }

private:
    void RenderCppxClientUiFrame(Surface &surface);
    // Bake curated legacy sprites → cppxChrome. rw/rh = device resolution, uiScale =
    // the logical-canvas content scale.
    void BakeChromeTextures(int rw, int rh, float uiScale, bool deferredPass);
    void BakeMapPreviews();
    client::ui::SessionPhase CurrentSessionPhase() const;

    Game &game;

    // cppx render path. Lazily created on first cppx frame.
    std::unique_ptr<silencer::cppx_ui::PipelineHost> cppxHost;
    const uint8_t *cppxUiRgba = nullptr;
    int cppxUiW = 0;
    int cppxUiH = 0;
    bool cppxUiUnchanged_ = false;
    const silencer::cppx_ui::GpuUiProgram *cppxUiProgram_ = nullptr;
    bool gpuUiFlagChecked_ = false;
    bool gpuUiFlagEnabled_ = false;
    bool UseGpuUi();
    bool cppxReactInitialized = false;
    bool cppxAppRootPushed = false;
    // Wall-clock of the previous UI frame, for the use_clock() delta.
    uint32_t cppxLastUiTicks_ = 0;
    client::ui::Clock cppxClock_ = {};
    // Logical UI canvas width this frame (height-pinned 720 space).
    float cppxCanvasW_ = 0.0f;

    // Previous projected phase, so a phase screen's back can return to it (phase
    // screens aren't on the ScreenStack).
    client::ui::SessionPhase sessionPhaseCurrent_{};
    client::ui::SessionPhase sessionPhasePrevious_{};

    // Baked legacy-sprite chrome ids, re-baked on a host renderer reset (resize).
    client::ui::ChromeTextures cppxChrome;
    std::map<uint64_t, uint32_t> hudRampVariants_;
    uint64_t lastHoveredAudible_ = 0;
    uint32_t uiClickCount_ = 0;
    std::map<uint16_t, client::ui::ChromeTextures::Sprite> hudEmblems_;
    bool chromeDeferredBaked_ = false;
    // True while an overlay swap's Out fade leg runs (held mutation waiting for black).
    bool overlayFadingOut_ = false;

    // Snapshot of the persisted prefs at the last commit/revert; live Config
    // diverging from it => Settings.dirty.
    struct CommittedSettings {
        bool music = true;
        uint8_t musicvolume = 48;
        bool fullscreen = true;
        bool scalefilter = true;
    };
    CommittedSettings committedSettings_ = {};
    bool committedSettingsInit_ = false;
    // use_key_map dirty flag (the live KeyMap has no dirty bit).
    bool keymapDirty_ = false;

    // Per-tick POD copies of read-state, captured under LockMutex (lobby) / on the
    // game thread (world) before the build, so the frame provider reads them lock-free.
    client::ui::LobbySnapshot lobbySnapshot_ = {};
    client::ui::WorldSessionSnapshot worldSessionSnapshot_ = {};
    // Bundled map choices for GameCreatePanel; disk-listed once.
    std::vector<std::string> bundledMaps_ = {};
    bool bundledMapsListed_ = false;
    // Baked minimap-preview textures (filename → texture_id), baked once.
    client::ui::MapPreviews cppxMapPreviews_;
    bool mapPreviewsBaked_ = false;

    // Per-frame UI input + derived pointer; cleared after the pipeline consumes them.
    ::ui::UiInputFrame uiInput_ = {};
    bool injectedPointer_ = false;
    float injectedPointerX_ = 0.0f;
    float injectedPointerY_ = 0.0f;
    bool hasInjectedHover_ = false;
    float injectedHoverX_ = 0.0f;
    float injectedHoverY_ = 0.0f;
    bool injectedPressHeld_ = false;
    bool injectedPressIsNew_ = false;
    float injectedPressX_ = 0.0f;
    float injectedPressY_ = 0.0f;
    bool prevPointerDown_ = false;
    bool textInputActive_ = false;
    PasswordModalResult passwordModal_ = {};

    // `active` gates events.cpp's raw-edge intake.
    struct KeybindCaptureState {
        bool active = false;
        Action targetAction = Action::MoveUp;
        int targetComboIndex = -1; // -1 = append a new combo
        std::vector<BindingKey> pending = {};
    };
    KeybindCaptureState keybindCapture_ = {};
};

#endif
