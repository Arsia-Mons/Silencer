#pragma once

#include "client/ui/hooks/use_updater.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the updater model. Innermost link of the global FrameProvider chain
// (doc §5). The composition root maps ::Updater state -> the model and supplies
// the intent closures over the public ::Updater methods.
struct UpdaterProviderValue {
  UpdaterModel updater = {};
};

::ui::UiElement UpdaterProvider(const UpdaterProviderValue &value,
                                ::ui::UiChildren children,
                                const char *key = nullptr);

} // namespace client::ui
