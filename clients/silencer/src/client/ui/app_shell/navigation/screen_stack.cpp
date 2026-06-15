#include "screen_stack.h"

namespace client::ui {

bool UiScreen::build_element(::ui::UiElementFrame &frame,
                             ::ui::UiElement *out) {
  (void)frame;
  (void)out;
  return false;
}

bool ScreenStack::resolve_fade(const UiScreen &screen, FadeOverride fade) {
  switch (fade) {
  case FadeOverride::Fade:
    return true;
  case FadeOverride::NoFade:
    return false;
  case FadeOverride::Default:
  default:
    return screen.wants_transition_fade();
  }
}

bool ScreenStack::consume_transition_fade() {
  bool pending = transition_fade_pending_;
  transition_fade_pending_ = false;
  return pending;
}

bool ScreenStack::push(std::unique_ptr<UiScreen> screen, FadeOverride fade) {
  if (!screen || count_ >= CLIENT_UI_MAX_SCREENS)
    return false;
  note_fade(resolve_fade(*screen, fade));
  screen->set_entry_id(next_entry_id_++);
  screens_[count_++] = std::move(screen);
  return true;
}

bool ScreenStack::pop_top() {
  if (count_ <= 0)
    return false;
  if (screens_[count_ - 1])
    note_fade(resolve_fade(*screens_[count_ - 1], FadeOverride::Default));
  screens_[--count_].reset();
  return true;
}

bool ScreenStack::pop_entry(UiScreenEntryId entry_id) {
  for (int i = count_ - 1; i >= 0; --i) {
    if (screens_[i] && screens_[i]->entry_id() == entry_id) {
      note_fade(resolve_fade(*screens_[i], FadeOverride::Default));
      for (int j = i; j + 1 < count_; ++j) {
        screens_[j] = std::move(screens_[j + 1]);
      }
      screens_[--count_].reset();
      return true;
    }
  }
  return false;
}

bool ScreenStack::replace_top(std::unique_ptr<UiScreen> screen,
                              FadeOverride fade) {
  if (!screen)
    return false;
  if (count_ <= 0)
    return push(std::move(screen), fade);
  note_fade(resolve_fade(*screen, fade));
  screen->set_entry_id(next_entry_id_++);
  screens_[count_ - 1] = std::move(screen);
  return true;
}

bool ScreenStack::reset_to(std::unique_ptr<UiScreen> screen, FadeOverride fade) {
  if (!screen)
    return false;
  for (int i = 0; i < count_; ++i) {
    screens_[i].reset();
  }
  count_ = 0;
  return push(std::move(screen), fade);
}

UiScreen *ScreenStack::at(int index) const {
  if (index < 0 || index >= count_)
    return nullptr;
  return screens_[index].get();
}

UiScreen *ScreenStack::top() const {
  return count_ > 0 ? screens_[count_ - 1].get() : nullptr;
}

::ui::Span<UiScreen *> ScreenStack::visible_screens() {
  int start = count_;
  for (int i = count_ - 1; i >= 0; --i) {
    start = i;
    if (screens_[i]->kind() == ScreenKind::Normal)
      break;
  }

  int visible_count = 0;
  for (int i = start; i < count_; ++i) {
    visible_[visible_count++] = screens_[i].get();
  }
  return {visible_.data(), visible_count};
}

} // namespace client::ui
