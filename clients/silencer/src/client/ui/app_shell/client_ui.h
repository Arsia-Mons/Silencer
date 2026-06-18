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
  const ::ui::DrawCommandList &retained_command_list() const {
    return retained_command_list_;
  }
  bool wants_text_input() const { return wants_text_input_; }

  // Per-frame interaction-audio edges exposed as DATA: the composition root
  // owns Audio, the UI side never plays sounds.
  struct UiAudioEvents {
    ::ui::NodeId hovered_button = 0;
    bool activated_button = false;
    bool nav_focused_button = false;
  };
  const UiAudioEvents &audio_events() const { return audio_events_; }

  void begin_frame(const ::ui::UiInputFrame &input);
  void build_visible_screens(const UiElementWrapper &wrap_root = {});
  void end_layout(const ::ui::UiInputFrame &input);
  bool update_retained_runtime(const ::ui::FlexLayoutAdapter &layout,
                               ::ui::LayoutViewport viewport,
                               const ::ui::InputFrame &input);

  // Frame-scoped cancel handler (the use_cancel seam). Keyed by entry_id so
  // only the cancel target's registration is honored; cleared each begin_frame.
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

  // Structural-mutation gating: when held, drain_deferred_mutations applies the
  // Deferred (domain) mutations but leaves stack-changing ones queued so the
  // composition root can gate the stack swap behind a transition fade.
  void set_structural_hold(bool held) { structural_hold_ = held; }
  bool has_pending_structural_mutations() const;
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
  bool is_pending_pop(UiScreenEntryId entry_id) const;
  // The screen a cancel edge acts on: the topmost not already queued for pop, so
  // mashing ESC mid-fade walks back instead of re-firing the held top screen.
  UiScreen *effective_cancel_target() const;

  ScreenStack screens_;
  ::ui::UiElementFrame retained_element_frame_ = {};
  ::ui::UiTree retained_tree_ = {};
  ::ui::FocusRuntime retained_focus_ = {};
  ::ui::DrawCommandList retained_command_list_ = {};
  UiAudioEvents audio_events_ = {};
  // The next three carry the PREVIOUS frame's focus-pass results, published to
  // the tree during the next build (one-frame lag, by design — styling §7).
  ::ui::InteractionSnapshot interaction_snapshot_ = {};
  ::ui::FocusScrollRequest focus_scroll_request_ = {};
  ::ui::MeasuredSizeRequest measured_sizes_ = {};
  // Persistent per-input horizontal scroll offsets (keyed by node id); owned
  // here so they survive across frames. The IR builder reads + updates them.
  ::ui::InputScrollStore input_scroll_ = {};
  ::ui::NodeId prev_hovered_node_ = 0;
  std::array<QueuedMutation, CLIENT_UI_MAX_QUEUED_MUTATIONS> mutations_ = {};
  int mutation_count_ = 0;
  bool structural_hold_ = false;
  bool wants_text_input_ = false;

  struct FrameCancelSlot {
    bool present = false;
    UiScreenEntryId entry_id = 0;
    std::function<void()> handler = {};
  };
  FrameCancelSlot cancel_slot_ = {};
};

} // namespace client::ui
