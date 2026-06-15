#pragma once

#include <functional>

namespace client::ui {

// The cancel/back seam (mirrors use_navigation). The TOP screen registers a
// cancel handler during build; ClientUi's central cancel pass (end_layout)
// invokes it on the cancel edge (ESC / controller back / the `back` op), else
// falls back to the default (pop the top overlay). The handler runs after build
// and before deferred mutations drain, so it must only QUEUE work (nav.push,
// chat.cancel(), …) — never touch the retained tree. Capture intent closures BY
// VALUE so they survive the per-frame arena reset.
//
// Reads the same NavigationContext as use_navigation (no new provider).
void use_cancel(std::function<void()> on_cancel);

} // namespace client::ui
