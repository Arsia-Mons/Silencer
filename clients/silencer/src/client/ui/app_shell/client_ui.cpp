#include "client_ui.h"

#include "client/ui/providers/navigation_provider.h"

#include <stdio.h>

namespace client::ui {

struct LegacyScreenBuildProps {
  UiScreen *screen = nullptr;
};

static const char *screen_provider_key(UiScreenEntryId entry_id) {
  char key[64] = {};
  snprintf(key, sizeof(key), "screen-provider-%u", entry_id);
  return ::ui::copy_string(key);
}

static const char *screen_layer_key(UiScreenEntryId entry_id) {
  char key[64] = {};
  snprintf(key, sizeof(key), "screen-layer-%u", entry_id);
  return ::ui::copy_string(key);
}

static ::ui::UiElement LegacyScreenBuild(const LegacyScreenBuildProps &props) {
  if (props.screen)
    props.screen->build_ui();
  return ::ui::empty();
}

ClientUi::ClientUi() { ::ui::focus_init(&retained_focus_); }

void ClientUi::begin_frame(const ::ui::UiInputFrame &input) {
  (void)input;
  clear_mutations();
  retained_element_frame_.reset();
}

void ClientUi::build_visible_screens(const UiElementWrapper &wrap_root) {
  ::ui::UiElementFrameScope frame_scope(retained_element_frame_);
  ::ui::Span<UiScreen *> visible = screens_.visible_screens();
  for (int i = 0; i < visible.count; ++i) {
    // Descriptors and copied props only need to live through the immediate
    // commit below. Reset per visible screen so overlay stacks do not exhaust
    // the transient frame before the top screen commits.
    retained_element_frame_.reset();
    UiScreen *screen = visible[i];
    if (!screen)
      continue;
    auto build_screen = [&] {
      NavigationProviderValue context = {
          .client_ui = this,
          .current_entry_id = screen->entry_id(),
          .is_top = i == visible.count - 1,
      };
      ::ui::UiElement root = {};
      if (!screen->build_element(retained_element_frame_, &root)) {
        root = ::ui::component(
            "LegacyScreenBuild", LegacyScreenBuildProps{.screen = screen},
            LegacyScreenBuild, screen_provider_key(screen->entry_id()));
      }
      {
        ::ui::UiElement provider = NavigationProvider(
            context, ::ui::children({root}), screen_provider_key(screen->entry_id()));
        // Publish last frame's interaction (one-frame lag, by design) as a
        // declarative provider so components resolve focus/hover/press.
        provider = ::ui::provider(
            "InteractionProvider", &::ui::InteractionContext,
            const_cast<::ui::InteractionSnapshot *>(&interaction_snapshot_),
            ::ui::children({provider}));
        if (wrap_root) {
          provider = wrap_root(provider);
        }
        // Composite every visible screen as a full-bleed ABSOLUTE layer. All
        // visible screens commit into one retained tree as direct children of
        // the root, whose default flex column would otherwise stack them
        // vertically (an overlay would land below the viewport). Absolute +
        // inset 0 makes each screen fill the viewport and OVERLAP; paint order
        // equals stack order, so overlays draw on top of the base phase screen.
        ::ui::HostProps layer = {};
        layer.key = screen_layer_key(screen->entry_id());
        layer.style.position = ::ui::PositionType::Absolute;
        layer.style.position_inset =
            ::ui::EdgeSizes::all_edges(::ui::StyleValue::points(0.0f));
        layer.style.width = ::ui::Length::percent(100.0f);
        layer.style.height = ::ui::Length::percent(100.0f);
        layer.children = ::ui::children({provider});
        ::ui::ReconcileResult result = ::ui::commit_retained_elements(
            retained_tree_, retained_element_frame_, ::ui::box(layer));
        if (!result.ok) {
          react_report_error(
              "client/ui: failed to commit returned screen %s (errors=%d)\n",
              screen->debug_name(), result.error_count);
        }
      }
    };

    switch (screen->kind()) {
    case ScreenKind::Normal:
      build_screen();
      break;
    case ScreenKind::Overlay:
      build_screen();
      break;
    }
  }
}

void ClientUi::end_layout(const ::ui::UiInputFrame &input) {
  UiScreen *top = screens_.top();
  if (input.cancel_pressed && top && top->kind() == ScreenKind::Overlay) {
    queue_pop_current(top->entry_id());
  }
}

bool ClientUi::update_retained_runtime(const ::ui::FlexLayoutAdapter &layout,
                                       ::ui::LayoutViewport viewport,
                                       const ::ui::InputFrame &input) {
  if (!::ui::compute_flex_layout(layout, retained_tree_, viewport))
    return false;
  if (!::ui::focus_update(&retained_focus_, retained_tree_, input))
    return false;
  ::ui::NodeId blurred = ::ui::focus_blurred_id(retained_focus_);
  if (blurred != 0) {
    retained_tree_.invoke_blur(blurred);
  }
  ::ui::NodeId focused = ::ui::focus_changed_id(retained_focus_);
  if (focused != 0) {
    retained_tree_.invoke_focus(focused);
  }
  ::ui::NodeId confirmed = ::ui::focus_confirmed_id(retained_focus_);
  if (confirmed != 0) {
    // Clicking a text input focuses it but never activates (origin: inputs
    // submit on RETURN only — a pointer click must not fire on_activate, or
    // every submit-on-activate field fires on focus-click).
    ::ui::NodeSnapshot cs = {};
    bool pointer_on_input = ::ui::focus_confirmed_by_pointer(retained_focus_) &&
                            retained_tree_.snapshot(confirmed, &cs) &&
                            cs.role == ::ui::NodeRole::Input;
    if (!pointer_on_input)
      retained_tree_.invoke_activate(confirmed);
  }

  // Interaction-audio edges (origin ClientUi.cpp:95-111): hovered audible
  // button + activate/keyboard-navigate onto one. Published as data; the
  // composition root dedupes the hover edge and plays.
  audio_events_ = {};
  auto audible_button = [&](::ui::NodeId id) {
    if (id == 0)
      return false;
    ::ui::NodeSnapshot s = {};
    return retained_tree_.snapshot(id, &s) && s.role == ::ui::NodeRole::Button &&
           !s.interaction.disabled;
  };
  ::ui::NodeId hovered_now = ::ui::focus_hovered_id(retained_focus_);
  // Hover enter/leave edges (React onMouseEnter/Leave analog): components
  // that animate on hover (the legacy oval ramp) track their own state via
  // these callbacks — use_hovered() can't serve them, since the host node's
  // fiber is the substrate Button's, not the product component's.
  if (hovered_now != prev_hovered_node_) {
    if (prev_hovered_node_ != 0)
      retained_tree_.invoke_hover(prev_hovered_node_, false);
    if (hovered_now != 0)
      retained_tree_.invoke_hover(hovered_now, true);
    prev_hovered_node_ = hovered_now;
  }
  if (audible_button(hovered_now))
    audio_events_.hovered_button = hovered_now;
  if (audible_button(confirmed))
    audio_events_.activated_button = true;
  if ((input.nav_up || input.nav_down || input.nav_left || input.nav_right) &&
      audible_button(focused))
    audio_events_.nav_focused_button = true;

  ::ui::NodeId active = ::ui::focus_focused_id(retained_focus_);
  for (int i = 0; i < input.key_event_count; ++i) {
    retained_tree_.invoke_key(active, input.key_events[i]);
  }
  // Scroll wheel routes to the node under the pointer, then BUBBLES up the
  // ancestor chain to the first node that handles it (its on_wheel) — mirrors
  // DOM wheel bubbling. The topmost hovered node is often a child control (a
  // button in a scrollable row) with no on_wheel; the scroll viewport that owns
  // the wheel is an ancestor (SIL-111).
  if (input.wheel_x != 0.0f || input.wheel_y != 0.0f) {
    ::ui::NodeId n = ::ui::focus_hovered_id(retained_focus_);
    while (n != 0) {
      if (retained_tree_.invoke_wheel(n, input.wheel_x, input.wheel_y))
        break;
      ::ui::NodeSnapshot s = {};
      if (!retained_tree_.snapshot(n, &s))
        break;
      n = s.parent_id;
    }
  }
  for (int i = 0; i < input.text_event_count; ++i) {
    retained_tree_.invoke_text_input(active, input.text_events[i]);
  }
  for (int i = 0; i < input.editing_event_count; ++i) {
    retained_tree_.invoke_text_editing(active, input.editing_events[i]);
  }

  ::ui::NodeSnapshot active_snapshot = {};
  wants_text_input_ =
      active != 0 && retained_tree_.snapshot(active, &active_snapshot) &&
      !active_snapshot.interaction.disabled &&
      (active_snapshot.role == ::ui::NodeRole::Input ||
       active_snapshot.semantic_role == ::ui::SemanticRole::TextBox);

  // Capture this frame's interaction, keyed by fiber, for next frame's build.
  auto fiber_of = [&](::ui::NodeId id) -> uint64_t {
    ::ui::NodeSnapshot s = {};
    return (id != 0 && retained_tree_.snapshot(id, &s)) ? s.fiber_id : 0;
  };
  interaction_snapshot_ = ::ui::InteractionSnapshot{
      .focused_fiber = fiber_of(::ui::focus_focused_id(retained_focus_)),
      .hovered_fiber = fiber_of(::ui::focus_hovered_id(retained_focus_)),
      .pressed_fiber = fiber_of(::ui::focus_pressed_id(retained_focus_)),
      .source = ::ui::focus_source(retained_focus_),
  };

  // Build the tagged-union IR that the live render path executes via
  // renderer::execute_draw_commands.
  return ::ui::build_draw_command_list(retained_tree_, &retained_command_list_,
                                       active);
}

bool ClientUi::push_screen(std::unique_ptr<UiScreen> screen) {
  return screens_.push(std::move(screen));
}

bool ClientUi::replace_top(std::unique_ptr<UiScreen> screen) {
  return screens_.replace_top(std::move(screen));
}

bool ClientUi::queue_push_screen(std::unique_ptr<UiScreen> screen) {
  if (!screen)
    return false;
  return queue_mutation({
      .kind = MutationKind::Push,
      .screen = std::move(screen),
  });
}

bool ClientUi::queue_reset_to_screen(std::unique_ptr<UiScreen> screen) {
  if (!screen)
    return false;
  return queue_mutation({
      .kind = MutationKind::ResetTo,
      .screen = std::move(screen),
  });
}

bool ClientUi::queue_pop_current(UiScreenEntryId entry_id) {
  return queue_mutation({
      .kind = MutationKind::PopCurrent,
      .entry_id = entry_id,
  });
}

bool ClientUi::queue_pop_top() {
  return queue_mutation({.kind = MutationKind::PopTop});
}

bool ClientUi::queue_deferred_mutation(DeferredUiMutation mutation) {
  if (!mutation)
    return false;
  return queue_mutation({
      .kind = MutationKind::Deferred,
      .deferred = std::move(mutation),
  });
}

bool ClientUi::queue_mutation(QueuedMutation mutation) {
  if (mutation_count_ >= CLIENT_UI_MAX_QUEUED_MUTATIONS)
    return false;
  mutations_[mutation_count_++] = std::move(mutation);
  return true;
}

void ClientUi::drain_deferred_mutations() {
  for (int i = 0; i < mutation_count_; ++i) {
    QueuedMutation &mutation = mutations_[i];
    switch (mutation.kind) {
    case MutationKind::Push:
      screens_.push(std::move(mutation.screen));
      break;
    case MutationKind::ResetTo:
      screens_.reset_to(std::move(mutation.screen));
      break;
    case MutationKind::PopCurrent:
      screens_.pop_entry(mutation.entry_id);
      break;
    case MutationKind::PopTop:
      screens_.pop_top();
      break;
    case MutationKind::Deferred:
      if (mutation.deferred)
        mutation.deferred();
      break;
    }
  }
  clear_mutations();
}

void ClientUi::clear_mutations() {
  for (int i = 0; i < mutation_count_; ++i) {
    mutations_[i] = {};
  }
  mutation_count_ = 0;
}

} // namespace client::ui
