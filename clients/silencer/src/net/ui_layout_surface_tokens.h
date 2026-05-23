#ifndef UI_LAYOUT_SURFACE_TOKENS_H
#define UI_LAYOUT_SURFACE_TOKENS_H

#include "ui_editor_preview_model.h"
#include "ui_layout_contract.generated.h"

#include <string>

namespace silencer::net {

const ui_layout_contract::SurfaceTokens * FindUiLayoutSurfaceTokens(
	const std::string& surface);

bool ValidateUiDocumentGeneratedSurfaceTokens(
	const silencer::ui::UiEditorPreviewDocument& document,
	std::string& error);

}  // namespace silencer::net

#endif
