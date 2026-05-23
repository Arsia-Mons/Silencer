#include "layout/ui_document_runtime_registry.h"

#include "main_menu/main_menu_document_runtime.h"
#include "options/options_document_runtime.h"

namespace silencer::client_ui {

UiDocumentRendererOptions UiDocumentRendererOptionsForSurface(
	const std::string& surface) {
	UiDocumentRendererOptions options;
	if(surface == main_menu::kMainMenuSurface){
		main_menu::ApplyMainMenuRuntimeHandlers(options);
	}else if(surface == options_menu::kOptionsSurface){
		options_menu::ApplyOptionsMenuRuntimeHandlers(options);
	}else if(surface == options_display::kOptionsDisplaySurface){
		options_display::ApplyOptionsDisplayRuntimeHandlers(options);
	}else if(surface == options_audio::kOptionsAudioSurface){
		options_audio::ApplyOptionsAudioRuntimeHandlers(options);
	}
	return options;
}

}  // namespace silencer::client_ui
