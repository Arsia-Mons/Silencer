#include "panel_components.h"

UiElement BuildMultiline(Actions &actions) {
#line 4 "tests/fixtures/cppx/multiline.cppx"
  return ::ui::component("Panel", PanelProps{ .key = "pause", .style = Style{ .width = Length::points(320), .height = Length::points(180), }, .modal = true, .children = children({
#line 12 "tests/fixtures/cppx/multiline.cppx"
    ::ui::component("Button", ButtonProps{ .id = "ResumeButton", .on_activate = actions.resume, .children = children({
#line 13 "tests/fixtures/cppx/multiline.cppx"
      text("Resume"),
#line 14 "tests/fixtures/cppx/multiline.cppx"
    }) }, Button),
#line 15 "tests/fixtures/cppx/multiline.cppx"
  }) }, Panel);
}
