#ifndef SILENCER_CLIENT_UI_LAYOUT_UI_DOCUMENT_RENDERER_H
#define SILENCER_CLIENT_UI_LAYOUT_UI_DOCUMENT_RENDERER_H

#include "primitives/button.h"
#include "ui_editor_preview_model.h"

#include <functional>
#include <string>

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui {

struct UiDocumentRendererOptions {
	silencer::ui::primitives::ButtonVariant buttonVariant =
		silencer::ui::primitives::ButtonVariant::Chrome;
	silencer::ui::primitives::ButtonSize buttonSize =
		silencer::ui::primitives::ButtonSize::Auto;
	std::function<bool(const silencer::ui::UiEditorNode&)> buildComponent;
	std::function<std::string(const std::string&)> resolveTextBinding;
};

void BuildUiDocument(const silencer::ui::UiEditorPreviewDocument& document,
                     silencer::ui::UiInteractionRegistry& interactions,
                     const UiDocumentRendererOptions& options = UiDocumentRendererOptions{});

void BuildUiDocumentNode(const silencer::ui::UiEditorNode& node,
                         silencer::ui::UiInteractionRegistry& interactions,
                         const UiDocumentRendererOptions& options = UiDocumentRendererOptions{});

}  // namespace silencer::client_ui

#endif
