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

  ::ui::UiTree &tree() { return tree_; }
  const ::ui::UiTree &tree() const { return tree_; }
  ::ui::FocusRuntime &focus() { return focus_; }
  const ::ui::DrawCommandList &command_list() const { return command_list_; }
  bool wants_text_input() const { return wants_text_input_; }

  void begin_frame(const ::ui::UiInputFrame &input);
  void build_visible_screens(const UiElementWrapper &wrap_root = {});
  void end_layout(const ::ui::UiInputFrame &input);
  bool update_runtime(const ::ui::FlexLayoutAdapter &layout,
                      ::ui::LayoutViewport viewport,
                      const ::ui::InputFrame &input);

  bool push_screen(std::unique_ptr<UiScreen> screen);
  bool replace_top(std::unique_ptr<UiScreen> screen);
  bool queue_push_screen(std::unique_ptr<UiScreen> screen);
  bool queue_reset_to_screen(std::unique_ptr<UiScreen> screen);
  bool queue_pop_current(UiScreenEntryId entry_id);
  bool queue_pop_top();
  bool queue_deferred_mutation(DeferredUiMutation mutation);
  int pending_mutation_count() const { return mutation_count_; }
  void drain_deferred_mutations();

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
    DeferredUiMutation deferred = {};
  };

  bool queue_mutation(QueuedMutation mutation);
  void clear_mutations();

  ScreenStack screens_;
  ::ui::UiElementFrame element_frame_ = {};
  ::ui::UiTree tree_ = {};
  ::ui::FocusRuntime focus_ = {};
  ::ui::DrawCommandList command_list_ = {};
  // Interaction state from the PREVIOUS frame's focus pass, published to the
  // component tree during the next build so use_focused()/use_hovered()/etc.
  // resolve (one-frame lag, by design — styling design §7).
  ::ui::InteractionSnapshot interaction_snapshot_ = {};
  std::array<QueuedMutation, CLIENT_UI_MAX_QUEUED_MUTATIONS> mutations_ = {};
  int mutation_count_ = 0;
  bool wants_text_input_ = false;
};

} // namespace client::ui
