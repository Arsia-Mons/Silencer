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

// Owns the live client UI for one frame. SIL-14/15: the golden retained cppx
// path is the only UI — RenderClientUiFrame drives the golden
// client::ui::UiPipeline (AppRoot + the global provider chain + the session-
// phase reconciler) through the SIL-11 PipelineHost, producing a premultiplied
// RGBA frame GameRenderer::Present hands to RenderDevice::UploadUiFrame. The
// legacy immediate-mode UI frame path + its screen-context bag were removed;
// DrawInGameWorldInsets is the only non-cppx survivor (it draws the in-game
// minimap / system-camera insets straight to the world Surface).
class GameUiPipeline {
public:
    explicit GameUiPipeline(Game &game);
    ~GameUiPipeline();

    void DrawInGameWorldInsets(Surface &surface, float frametime);
    void RenderClientUiFrame(Surface &surface, float frametime);

    // The cppx RGBA produced this frame (w*h*4, premultiplied), or null when the
    // cppx path did not render this frame. Owned by the PipelineHost; valid until
    // the next RenderClientUiFrame.
    const uint8_t *CppxUiFrame(int &outW, int &outH) const;

    // SIL-237: true when this frame's draw-command IR was byte-identical to the
    // previous one, so the PipelineHost skipped the raster and CppxUiFrame returns
    // the SAME pixels as last frame. The caller may then skip the GPU UI upload —
    // but ONLY when the fade alpha it applies at upload time is also unchanged (the
    // fade is not in the IR). False whenever the raster ran (or no cppx frame yet).
    bool CppxUiUnchanged() const { return cppxUiUnchanged_; }

    // SIL-240: the GPU UI geometry program built this frame (windowed GPU path), or
    // null when the cppx path produced a CPU raster instead (headless/test) or did
    // not render. Owned by the PipelineHost; valid until the next RenderClientUiFrame.
    // GameRenderer::Present submits this to the backend instead of UploadUiFrame.
    const silencer::cppx_ui::GpuUiProgram *CppxUiProgram() const { return cppxUiProgram_; }

    // --- UI input + control-socket automation seam (SIL-18) ----------------
    // The per-frame UI input frame. The single SDL-event site (events.cpp, windowed)
    // and the control socket (headless automation) accumulate nav/confirm/cancel/
    // key/text edges here; RenderCppxClientUiFrame consumes it, then clears the
    // one-frame edges.
    ::ui::UiInputFrame &UiInput() { return uiInput_; }
    // True while the focused cppx node is a text field (mirrors the SDL text-input
    // gate). events.cpp reads it to keep SPACE out of the confirm/activate edge
    // when typing — a focused field types a space; only Enter submits.
    bool WantsTextInput() const { return textInputActive_; }
    // Inject a single-frame pointer click (press+release) at a UI-space point, so
    // the control socket can activate a node by location.
    void InjectPointerClick(float x, float y);
    // Park a sticky pointer position at a UI-space point (no press) so the control
    // socket can drive focus-follows-hover headlessly (there is no real mouse). The
    // position persists across frames until moved again; click injection is still a
    // one-frame edge and does not disturb it.
    void InjectPointerMove(float x, float y);
    // SIL-223: hold the pointer DOWN at a UI-space point across frames (press edge
    // on the first frame, then a sustained `pointer_down`) so the held PRESSED
    // interaction state can be screenshotted. Released by InjectPointerRelease.
    // Test affordance only (control socket); no real mouse exists headless.
    void InjectPointerPress(float x, float y);
    // SIL-223: release a held pointer press, emitting the release edge next frame.
    void InjectPointerRelease();
    // The live retained UI tree owner, or null before the first cppx frame. The
    // control socket reads it for introspection (read-only tree + focus snapshots,
    // no friend/handle leak).
    client::ui::ClientUi *TryClientUi();
    // Push the props-only modal overlays (control socket today; screen-driven flows
    // in later slices). Direct (non-queued) push: it adds to the screen stack
    // immediately, surviving the next frame's begin_frame (which clears only the
    // mutation queue).
    void ShowPasswordModal(const char *title);
    void ShowMessageModal(const char *title, const char *message);
    // SIL-24: push the design-system gallery overlay (visual-regression harness;
    // automation/debug only — not reachable from the product UI).
    void ShowGallery();
    // The latest password-modal interaction, for the control socket's
    // password_modal_result op.
    struct PasswordModalResult {
        bool open = false;
        bool submitted = false;
        std::string value;
    };
    const PasswordModalResult &PasswordModal() const { return passwordModal_; }

    // --- Keybind capture (SIL-19 §7b) --------------------------------------
    // The capture state machine. Raw multi-device edges arrive in the game layer
    // (events.cpp, gated on IsCapturingKeybind) and the control socket; the UI
    // (use_keybind_capture) drives begin/confirm/cancel and renders the pending
    // chord. Live here (not in React state) because the edge source is global.
    bool IsCapturingKeybind() const { return keybindCapture_.active; }
    void BeginKeybindCapture(Action action, int comboIndex);
    // Append a raw edge to the pending chord (dedup; capped at CHORD_CAP). Returns
    // true when the chip was added. No-op when not capturing.
    bool FeedKeybindEdge(const BindingKey &key);
    // Commit the pending chord as the target combo (fork-if-builtin, caps), then
    // leave capture mode. No-op if the chord is empty.
    void ConfirmKeybindChord();
    void CancelKeybindCapture();
    const std::vector<BindingKey> &KeybindCapturePending() const {
        return keybindCapture_.pending;
    }

    // In-game HUD per-frame sprite variants (origin draw-time Effect* on indexed
    // sprites): pulse ramps (EffectRampColorPlus) + team-colorized emblems.
    // Lazily baked + memoized; invalidated with the chrome bake. 0 = not bakeable
    // yet (screens tolerate per the use_chrome contract).
    uint32_t EnsureHudRampVariant(uint8_t bank, uint16_t index, uint8_t rampColor,
                                  uint8_t rampPlus, uint8_t brightness = 128);
    client::ui::ChromeTextures::Sprite EnsureHudTeamEmblem(uint8_t agency,
                                                           uint8_t color);

    // UI interaction-click edges fired so far (hover-enter / activate / nav onto
    // an audible button — origin PlayMenuButtonSound triggers). Counted even when
    // audio is disabled (headless) so e2e can assert the edges.
    uint32_t UiClickCount() const { return uiClickCount_; }

private:
    void RenderCppxClientUiFrame(Surface &surface);
    // SIL-87: bake curated legacy sprites → cppxChrome. rw/rh = device resolution,
    // uiScale = the logical-canvas content scale (device-res element bakes derive
    // their logical draw rect from it).
    void BakeChromeTextures(int rw, int rh, float uiScale, bool deferredPass);
    // SIL-216: decompress each bundled map's stored 172x62 minimap and bake it to a
    // preview texture_id (keyed by filename), once. Runs right after the bundled-map
    // list is resolved; the ids travel to screens via the MapPreviewsProvider.
    void BakeMapPreviews();
    client::ui::SessionPhase CurrentSessionPhase() const;

    Game &game;

    // cppx render path (SIL-14). Lazily created on first cppx frame.
    std::unique_ptr<silencer::cppx_ui::PipelineHost> cppxHost;
    const uint8_t *cppxUiRgba = nullptr;
    int cppxUiW = 0;
    int cppxUiH = 0;
    // SIL-237: set each frame in RenderCppxClientUiFrame — true when the raster was
    // skipped because the IR was unchanged. Drives the caller's upload-skip.
    bool cppxUiUnchanged_ = false;
    // SIL-240: the GPU UI program built this frame (windowed GPU path) + the gate.
    const silencer::cppx_ui::GpuUiProgram *cppxUiProgram_ = nullptr;
    bool gpuUiFlagChecked_ = false;
    bool gpuUiFlagEnabled_ = false;
    // True when the active backend draws the UI on the GPU and the geometry path is
    // enabled (SILENCER_GPU_UI during the SIL-240 staging; default once complete).
    bool UseGpuUi();
    bool cppxReactInitialized = false;
    bool cppxAppRootPushed = false;
    // SIL-94: monotonic wall-clock of the previous UI frame, for the use_clock()
    // delta. 0 until the first frame. The Clock value is snapshotted once per
    // render frame (the frame-provider lambda runs once per screen layer).
    uint32_t cppxLastUiTicks_ = 0;
    client::ui::Clock cppxClock_ = {};
    // Logical UI canvas width of the current frame (height-pinned 720 space);
    // published to screens via the AppProvider (use_app().canvas_w).
    float cppxCanvasW_ = 0.0f;

    // Previous projected phase, so a phase screen's back can return to it (phase
    // screens aren't on the ScreenStack, so there's nothing to pop).
    client::ui::SessionPhase sessionPhaseCurrent_{};
    client::ui::SessionPhase sessionPhasePrevious_{};

    // SIL-87: baked legacy-sprite chrome ids, re-baked when the host resets its
    // renderer (resize). Populated after ensure(), read by the ChromeTexturesProvider
    // in the per-frame provider chain.
    client::ui::ChromeTextures cppxChrome;
    // Memoized in-game HUD sprite variants (see EnsureHud*); cleared on rebake.
    std::map<uint64_t, uint32_t> hudRampVariants_;
    // Interaction-audio state: last hovered audible button (edge dedupe, origin
    // hoveredAudioInteractableId_) + the click-edge counter.
    uint64_t lastHoveredAudible_ = 0;
    uint32_t uiClickCount_ = 0;
    std::map<uint16_t, client::ui::ChromeTextures::Sprite> hudEmblems_;
    bool chromeDeferredBaked_ = false;
    // SIL-225: overlay push/pop (Options + its submenus, pause, modals are pushed
    // via use_navigation, never GoToState) bypasses the GoToState fade. The stack
    // swap is gated behind a transition fade in RenderClientUiFrame; this latch is
    // true while the Out leg runs (held mutation waiting for black). See the
    // overlay-fade orchestration there.
    bool overlayFadingOut_ = false;

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
    // SIL-21 (4/n) in-match snapshot: the per-tick POD copy of the viewed agent +
    // match read-state, captured on the game thread before the build phase (doc §5).
    // The frame provider reads this when assembling the WorldSessionProvider value.
    client::ui::WorldSessionSnapshot worldSessionSnapshot_ = {};
    // SIL-21 (3/n) create-form map choices: the bundled maps the GameCreatePanel
    // offers. Disk-listed once (maps don't change at runtime), then folded into the
    // lobby snapshot each lobby-phase frame.
    std::vector<std::string> bundledMaps_ = {};
    bool bundledMapsListed_ = false;
    // SIL-216: baked minimap-preview textures (filename → texture_id), baked once
    // when the bundled-map list is resolved. Read by the MapPreviewsProvider in the
    // per-frame provider chain; the Create-Game map list shows the hovered map's.
    client::ui::MapPreviews cppxMapPreviews_;
    bool mapPreviewsBaked_ = false;

    // SIL-18 UI input: the per-frame edges (events.cpp + control socket) and the
    // derived pointer state. Cleared each frame after the pipeline consumes them.
    ::ui::UiInputFrame uiInput_ = {};
    bool injectedPointer_ = false;
    float injectedPointerX_ = 0.0f;
    float injectedPointerY_ = 0.0f;
    // Sticky injected hover position (headless focus-follows-hover automation): once
    // set via InjectPointerMove it drives `frame.pointer` every frame (no real mouse
    // in headless) until moved again.
    bool hasInjectedHover_ = false;
    float injectedHoverX_ = 0.0f;
    float injectedHoverY_ = 0.0f;
    // SIL-223 held-press automation: a sticky pointer-down at (x,y) that persists
    // across frames (press edge first frame, sustained down after) until released.
    bool injectedPressHeld_ = false;
    bool injectedPressIsNew_ = false;
    float injectedPressX_ = 0.0f;
    float injectedPressY_ = 0.0f;
    bool prevPointerDown_ = false;
    bool textInputActive_ = false;
    PasswordModalResult passwordModal_ = {};

    // SIL-19 keybind capture state. `active` gates events.cpp's raw-edge intake.
    struct KeybindCaptureState {
        bool active = false;
        Action targetAction = Action::MoveUp;
        int targetComboIndex = -1;            // -1 = append a new combo
        std::vector<BindingKey> pending = {}; // the chord under construction
    };
    KeybindCaptureState keybindCapture_ = {};
};

#endif
