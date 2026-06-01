#pragma once

template <typename Children>
UiElement ComposeLoadout(Children children) {
#line 5 "tests/fixtures/cppx/components.hx"
  return ::ui::component("Loadout.Frame", Loadout::FrameProps{ .key = "loadout", .children = children({
#line 6 "tests/fixtures/cppx/components.hx"
    ::ui::component("Loadout.Toolbar", Loadout::ToolbarProps{ .active_tab = state.active_tab }, Loadout::Toolbar),
#line 7 "tests/fixtures/cppx/components.hx"
    ::ui::component("Loadout.Content", Loadout::ContentProps{ .children = children({
#line 7 "tests/fixtures/cppx/components.hx"
    children(),
#line 7 "tests/fixtures/cppx/components.hx"
    }) }, Loadout::Content),
#line 8 "tests/fixtures/cppx/components.hx"
  }) }, Loadout::Frame);
}
