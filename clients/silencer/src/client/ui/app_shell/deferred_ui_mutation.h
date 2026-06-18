#pragma once

#include "client_ui.h"

namespace client::ui::internal {

// Low-level bridge for hooks that must schedule state changes after the UI
// declaration pass. Product UI should expose named actions instead of this.
struct DeferredUiMutationSink {
    ClientUi *client_ui = nullptr;

    bool submit(DeferredUiMutation mutation) const;
    const void *owner() const { return client_ui; }
    explicit operator bool() const { return client_ui != nullptr; }
};

DeferredUiMutationSink use_deferred_ui_mutations();

} // namespace client::ui::internal
