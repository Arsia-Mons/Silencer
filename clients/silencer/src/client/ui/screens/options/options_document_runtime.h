#ifndef OPTIONS_DOCUMENT_RUNTIME_H
#define OPTIONS_DOCUMENT_RUNTIME_H

#include "layout/ui_document_renderer.h"
#include "ui_layout_contract.generated.h"

#include <string>

namespace silencer::client_ui::options_menu {

constexpr const char * kOptionsSurface =
	silencer::net::ui_layout_contract::kUiSurfaceOptions;
constexpr const char * kActionControls =
	silencer::net::ui_layout_contract::kUiActionOptionsControls;
constexpr const char * kActionDisplay =
	silencer::net::ui_layout_contract::kUiActionOptionsDisplay;
constexpr const char * kActionAudio =
	silencer::net::ui_layout_contract::kUiActionOptionsAudio;
constexpr const char * kActionBack =
	silencer::net::ui_layout_contract::kUiActionOptionsBack;

}  // namespace silencer::client_ui::options_menu

namespace silencer::client_ui::options_display {

constexpr const char * kOptionsDisplaySurface =
	silencer::net::ui_layout_contract::kUiSurfaceOptionsDisplay;
constexpr const char * kComponentFullscreenIndicator =
	silencer::net::ui_layout_contract::kUiComponentOptionsDisplayFullscreenIndicator;
constexpr const char * kComponentSmoothScalingIndicator =
	silencer::net::ui_layout_contract::kUiComponentOptionsDisplaySmoothScalingIndicator;
constexpr const char * kActionFullscreen =
	silencer::net::ui_layout_contract::kUiActionOptionsDisplayFullscreen;
constexpr const char * kActionSmoothScaling =
	silencer::net::ui_layout_contract::kUiActionOptionsDisplaySmoothScaling;
constexpr const char * kActionSave =
	silencer::net::ui_layout_contract::kUiActionOptionsDisplaySave;
constexpr const char * kActionCancel =
	silencer::net::ui_layout_contract::kUiActionOptionsDisplayCancel;

bool IsOptionsDisplayComponent(const std::string& component);
void ApplyOptionsDisplayRuntimeHandlers(UiDocumentRendererOptions& options);

}  // namespace silencer::client_ui::options_display

namespace silencer::client_ui::options_audio {

constexpr const char * kOptionsAudioSurface =
	silencer::net::ui_layout_contract::kUiSurfaceOptionsAudio;
constexpr const char * kComponentMusicIndicator =
	silencer::net::ui_layout_contract::kUiComponentOptionsAudioMusicIndicator;
constexpr const char * kActionMusic =
	silencer::net::ui_layout_contract::kUiActionOptionsAudioMusic;
constexpr const char * kActionSave =
	silencer::net::ui_layout_contract::kUiActionOptionsAudioSave;
constexpr const char * kActionCancel =
	silencer::net::ui_layout_contract::kUiActionOptionsAudioCancel;

bool IsOptionsAudioComponent(const std::string& component);
void ApplyOptionsAudioRuntimeHandlers(UiDocumentRendererOptions& options);

}  // namespace silencer::client_ui::options_audio

#endif
