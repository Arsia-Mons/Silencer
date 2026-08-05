#pragma once

// The launcher's small shared components — the pieces every screen region is
// built from. They wrap the generic ui::components primitives with the
// launcher's tokens so a view never spells out paint.
//
// Every pressable surface lives in app_button.hx instead: one component, one
// `variant` prop.
//
// `const char *` text props are frame-owned: pass a literal, or dup() a
// computed std::string.

#include "ui/tokens.h"

#include "ui/runtime/element.h"

#include <functional>

namespace launcher {

struct LabelProps {
  const char *key = nullptr;
  const char *value = "";
  Color color = kText;
  float size = 11.0f;
  uint16_t face = kFaceBody;
  TextAlign align = TextAlign::Left;
  ::ui::TextWrap wrap = ::ui::TextWrap::None;
  // Wrapping needs a definite points width — Yoga does not wrap at a percent.
  Length width = Length::auto_size();
};

::ui::UiElement Label(const LabelProps &props);

// Status pip: filled when the thing exists, hollow when it does not.
struct DotProps {
  const char *key = nullptr;
  Color color = kOnline;
  bool hollow = false;
};

::ui::UiElement Dot(const DotProps &props);

struct DividerProps {
  const char *key = nullptr;
};

::ui::UiElement Divider(const DividerProps &props);

// Flex filler that pushes what follows to the far edge.
struct SpacerProps {
  const char *key = nullptr;
};

::ui::UiElement Spacer(const SpacerProps &props);

// One bullet of prose: a hanging marker beside wrapped body text. Shared by
// the news list blocks and the release notes.
struct BulletProps {
  const char *key = nullptr;
  const char *marker = "-";
  const char *value = "";
  float width = kDetailTextW - 20.0f;
};

::ui::UiElement Bullet(const BulletProps &props);

// Whole-pane message: loading, failed, or empty.
struct CenteredNoteProps {
  const char *key = nullptr;
  const char *value = "";
  Color color = kTextFaint;
};

::ui::UiElement CenteredNote(const CenteredNoteProps &props);

// One entry in the NEWS/RELEASES list column.
struct ListRowProps {
  const char *key = nullptr;
  const char *title = "";
  const char *marker = ""; // PINNED / LATEST, right-aligned on the sub line
  const char *sub = "";
  bool selected = false;
  std::function<void(const ::ui::ActivationEvent &)> on_activate = {};
};

::ui::UiElement ListRow(const ListRowProps &props);

// The 220px scrolling list beside a DetailPane. `key` also ids the viewport.
struct ListColumnProps {
  const char *key = nullptr;
  ::ui::UiChildren children = {};
};

::ui::UiElement ListColumn(const ListColumnProps &props);

// The scrolling read pane. Takes `height: 100%`, so it only resolves inside a
// flex row — see settings_tab.cppx, which wraps it in one for that alone.
struct DetailPaneProps {
  const char *key = nullptr;
  ::ui::UiChildren children = {};
};

::ui::UiElement DetailPane(const DetailPaneProps &props);

} // namespace launcher
