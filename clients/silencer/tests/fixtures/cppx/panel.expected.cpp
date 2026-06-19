#include "panel_components.h"

UiElement BuildPauseMenu(Actions &actions) {
#line 4 "tests/fixtures/cppx/panel.cppx"
  return ::ui::component("Panel", PanelProps{ .key = "pause", .style = Style{.width = Length::points(320)}, .modal = true, .children = children({
#line 5 "tests/fixtures/cppx/panel.cppx"
    ::ui::component("Panel.Header", Panel::HeaderProps{ .children = children({
#line 6 "tests/fixtures/cppx/panel.cppx"
      ::ui::component("Text", TextProps{ .value = "Paused" }, Text),
#line 7 "tests/fixtures/cppx/panel.cppx"
    }) }, Panel::Header),
#line 8 "tests/fixtures/cppx/panel.cppx"
    ::ui::component("Panel.Body", Panel::BodyProps{ .children = children({
#line 9 "tests/fixtures/cppx/panel.cppx"
      ::ui::component("Button", ButtonProps{ .id = "ResumeButton", .on_activate = actions.resume, .children = children({
#line 9 "tests/fixtures/cppx/panel.cppx"
      text("Resume"),
#line 9 "tests/fixtures/cppx/panel.cppx"
      }) }, Button),
#line 10 "tests/fixtures/cppx/panel.cppx"
      ::ui::component("Button", ButtonProps{ .id = "OptionsButton", .on_activate = actions.options }, Button),
#line 11 "tests/fixtures/cppx/panel.cppx"
    }) }, Panel::Body),
#line 12 "tests/fixtures/cppx/panel.cppx"
  }) }, Panel);
}
