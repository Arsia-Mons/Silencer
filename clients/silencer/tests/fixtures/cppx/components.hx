#pragma once

template <typename Children>
UiElement ComposeLoadout(Children children) {
  return <Loadout.Frame key="loadout">
    <Loadout.Toolbar activeTab={state.active_tab} />
    <Loadout.Content>{children()}</Loadout.Content>
  </Loadout.Frame>
}
