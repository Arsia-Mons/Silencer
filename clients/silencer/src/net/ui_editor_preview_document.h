#ifndef UI_EDITOR_PREVIEW_DOCUMENT_H
#define UI_EDITOR_PREVIEW_DOCUMENT_H

#include "ui_editor_preview_model.h"

#include "nlohmann/json.hpp"

#include <string>

namespace silencer::net {

bool ParseUiEditorPreviewDocument(const nlohmann::json& json,
                                  silencer::ui::UiEditorPreviewDocument& document,
                                  std::string& error);

}  // namespace silencer::net

#endif
