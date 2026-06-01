#pragma once

#include "client/ui/hooks/use_session.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the session model to the component tree. Mounted feature-global in
// the FrameProvider chain; the composition root assembles the `Session`
// (read projection + intent closures over the public Game seam) once per tick
// and hands it in. The provider holds no game handle — only the resolved model
// (doc §5).
struct SessionProviderValue {
  Session session = {};
};

::ui::UiElement SessionProvider(const SessionProviderValue &value,
                                ::ui::UiChildren children,
                                const char *key = nullptr);

} // namespace client::ui
