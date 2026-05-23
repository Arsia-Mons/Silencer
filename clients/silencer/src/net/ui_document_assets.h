#ifndef UI_DOCUMENT_ASSETS_H
#define UI_DOCUMENT_ASSETS_H

#include "ui_editor_preview_model.h"

#include <string>

namespace silencer::net {

bool LoadUiDocumentAsset(const std::string& surface,
                         silencer::ui::UiEditorPreviewDocument& document,
                         std::string& error);

bool ValidateUiDocumentKnownSurfaceTokens(
	const silencer::ui::UiEditorPreviewDocument& document,
	std::string& error);

}  // namespace silencer::net

#endif
