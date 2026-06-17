#pragma once

#include <array>
#include <functional>
#include <memory>

#include "../../../ui/runtime/react.h"
#include "../../../ui/input.h"
#include "../../../ui/runtime/draw_command.h"
#include "../../../ui/runtime/draw_command_builder.h"
#include "../../../ui/runtime/element.h"
#include "../../../ui/runtime/flex_layout.h"
#include "../../../ui/runtime/focus.h"
#include "../../../ui/runtime/interaction_hooks.h"
#include "../../../ui/runtime/tree.h"
#include "navigation/screen_stack.h"

namespace client::ui {

constexpr int CLIENT_UI_MAX_QUEUED_MUTATIONS = 128;

using DeferredUiMutation = std::function<void()>;
using UiElementWrapper = std::function<::ui::UiElement(::ui::UiElement child)>;

class ClientUi {
public:
  ClientUi();

  ScreenStack &screens() { return screens_; }
  const ScreenStack &screens() const { return screens_; }

  ::ui::UiTree &retained_tree() { return retained_tree_; }
  const ::ui::UiTree &retained_tree() const { return retained_tree_; }
  ::ui::FocusRuntime &retained_focus() { return retained_focus_; }
  // The tagged-union IR built each frame; the live render path executes it via
  // renderer::execute_draw_commands.
  const ::ui::DrawCommandList &retained_command_list() const {
    return retained_command_list_;
  }
  bool wants_text_input() const { return wants_text_input_; }

  // Per-frame interaction-audio edges (origin ClientUi PlayMenuButtonSound
  // triggers), exposed as DATA: the composition root owns Audio — the UI side
  // never plays sounds. Audible = an enabled Button (origin: toggles/inputs
  // are silent).
  struct UiAudioEvents {
    ::ui::NodeId hovered_button = 0; // button under the pointer (no edge dedupe)
    bool activated_button = false;   // confirm/click landed on a button
    bool nav_focused_button = false; // keyboard nav moved focus onto a button
  };
  const UiAudioEvents &audio_events() const { return audio_events_; }

  void begin_frame(const ::ui::UiInputFrame &input);
  void build_visible_screens(const UiElementWrapper &wrap_root = {});
  void end_layout(const ::ui::UiInputFrame &input);
  bool update_retained_runtime(const ::ui::FlexLayoutAdapter &layout,
                               ::ui::LayoutViewport viewport,
                               const ::ui::InputFrame &input);

  // Frame-scoped cancel handler (the use_cancel seam). The top screen
  // registers its handler during build; end_layout invokes it on the cancel
  // edge, else falls back to the default overlay pop. Last-writer-wins, keyed
  // by entry_id so only the TOP screen's registration is honored (a stale base
  // registration under an overlay is ignored). Cleared each begin_frame.
  void register_frame_cancel_handler(UiScreenEntryId entry_id,
                                     std::function<void()> handler);

  bool push_screen(std::unique_ptr<UiScreen> screen);
  bool replace_top(std::unique_ptr<UiScreen> screen);
  bool queue_push_screen(std::unique_ptr<UiScreen> screen,
                         FadeOverride fade = FadeOverride::Default);
  bool queue_reset_to_screen(std::unique_ptr<UiScreen> screen,
                             FadeOverride fade = FadeOverride::Default);
  bool queue_pop_current(UiScreenEntryId entry_id);
  bool queue_pop_top();
  bool queue_deferred_mutation(DeferredUiMutation mutation);
  int pending_mutation_count() const { return mutation_count_; }
  void drain_deferred_mutations();

  // Structural-mutation gating. When held, drain_deferred_mutations applies the
  // domain (Deferred) mutations but leaves stack-changing mutations
  // (Push/ResetTo/Pop*) queued, so the composition root can gate the visible
  // stack swap behind a transition fade. commit_structural_mutations applies
  // the held entries regardless of the hold. has_pending_structural_mutations
  // reports whether any are waiting.
  void set_structural_hold(bool held) { structural_hold_ = held; }
  bool has_pending_structural_mutations() const;
  // Whether the first queued stack swap wants the transition fade replayed
  // (per-push FadeOverride, else the affected screen's wants_transition_fade()).
  // The composition root reads this before committing to choose a full out->in
  // fade vs an instant cut (e.g. the in-match PauseScreen).
  bool pending_structural_wants_fade() const;
  void commit_structural_mutations();

private:
  enum class MutationKind {
    Push,
    ResetTo,
    PopCurrent,
    PopTop,
    Deferred,
  };

  struct QueuedMutation {
    MutationKind kind = MutationKind::PopTop;
    UiScreenEntryId entry_id = 0;
    std::unique_ptr<UiScreen> screen = nullptr;
    FadeOverride fade = FadeOverride::Default;
    DeferredUiMutation deferred = {};
  };

  bool queue_mutation(QueuedMutation mutation);
  void clear_mutations();
  void apply_mutation(QueuedMutation &mutation);
  static bool is_structural(MutationKind kind);
  // Whether a PopCurrent for this entry is already queued (held behind the
  // transition fade). Lets the cancel router skip screens that are already on
  // their way out so rapid cancels chain DOWN the stack.
  bool is_pending_pop(UiScreenEntryId entry_id) const;
  // The screen a cancel edge should act on: the topmost screen not already
  // queued for pop. With nothing pending this is just the top screen; mid-fade
  // it is the next screen down, so mashing ESC walks back instead of re-firing
  // the still-held top screen.
  UiScreen *effective_cancel_target() const;

  ScreenStack screens_;
  ::ui::UiElementFrame retained_element_frame_ = {};
  ::ui::UiTree retained_tree_ = {};
  ::ui::FocusRuntime retained_focus_ = {};
  // The IR list driving the live render path, built each frame in
  // update_retained_runtime.
  ::ui::DrawCommandList retained_command_list_ = {};
  UiAudioEvents audio_events_ = {};
  // Interaction state from the PREVIOUS frame's focus pass, published to the
  // component tree during the next build so use_focused()/use_hovered()/etc.
  // resolve (one-frame lag, by design — styling design §7).
  ::ui::InteractionSnapshot interaction_snapshot_ = {};
  // SIL-213: the focused node's scroll-into-view request from the PREVIOUS
  // frame's focus pass, published to the tree so the owning ScrollView reacts
  // (one-frame lag, by design — matches interaction_snapshot_).
  ::ui::FocusScrollRequest focus_scroll_request_ = {};
  // Layout measurement read-back: the PREVIOUS frame's post-layout node heights
  // (keyed by control id), published to the tree so a flex-grown component can
  // read its own resolved size (one-frame lag, by design — matches
  // focus_scroll_request_).
  ::ui::MeasuredSizeRequest measured_sizes_ = {};
  // Persistent per-input horizontal scroll offsets (keyed by node id) so a
  // focused single-line field scrolls minimally — the window moves only when the
  // caret leaves it. Owned here so it survives across frames; the IR builder
  // reads + updates it. (See draw_command_builder.h InputScrollStore.)
  ::ui::InputScrollStore input_scroll_ = {};
  // Last frame's hovered node — drives the on_hover enter/leave edge dispatch.
  ::ui::NodeId prev_hovered_node_ = 0;
  std::array<QueuedMutation, CLIENT_UI_MAX_QUEUED_MUTATIONS> mutations_ = {};
  int mutation_count_ = 0;
  bool structural_hold_ = false;
  bool wants_text_input_ = false;

  // The use_cancel registration for this frame (last-writer-wins).
  struct FrameCancelSlot {
    bool present = false;
    UiScreenEntryId entry_id = 0;
    std::function<void()> handler = {};
  };
  FrameCancelSlot cancel_slot_ = {};
};

} // namespace client::ui
