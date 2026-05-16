#ifndef SILENCER_CLIENT_UI_OPTIONS_COMPONENTS_BOOLEAN_SETTING_ROW_H
#define SILENCER_CLIENT_UI_OPTIONS_COMPONENTS_BOOLEAN_SETTING_ROW_H

#include "clay/clay.h"

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::options {

void BooleanSettingRow(Clay_String id,
                       Clay_String buttonId,
                       Clay_String label,
                       bool selected,
                       const char * actionId,
                       silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::options

#endif
