#ifndef SILENCER_CLIENT_UI_LAYOUT_UI_DOCUMENT_RUNTIME_REGISTRY_H
#define SILENCER_CLIENT_UI_LAYOUT_UI_DOCUMENT_RUNTIME_REGISTRY_H

#include "layout/ui_document_renderer.h"

#include <string>

namespace silencer::client_ui {

UiDocumentRendererOptions UiDocumentRendererOptionsForSurface(
	const std::string& surface);

}  // namespace silencer::client_ui

#endif
