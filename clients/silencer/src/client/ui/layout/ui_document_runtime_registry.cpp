#include "layout/ui_document_runtime_registry.h"

#include "ui_layout_surface_tokens.h"

namespace silencer::client_ui {

UiDocumentRendererOptions UiDocumentRendererOptionsForSurface(
	const std::string& surface) {
	UiDocumentRendererOptions options;
	options.canBuildComponent = [surface](const std::string& component) {
		return silencer::net::UiLayoutSurfaceAllowsComponent(surface, component);
	};
	options.canResolveTextBinding = [surface](const std::string& binding) {
		return silencer::net::UiLayoutSurfaceAllowsTextBinding(surface, binding);
	};
	options.canHandleAction = [surface](const std::string& action) {
		return silencer::net::UiLayoutSurfaceAllowsAction(surface, action);
	};
	return options;
}

}  // namespace silencer::client_ui
