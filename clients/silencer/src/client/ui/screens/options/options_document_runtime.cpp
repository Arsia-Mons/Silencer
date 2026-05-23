#include "options_document_runtime.h"

#include "components/boolean_setting_row.h"
#include "config.h"

#include <cstdint>
#include <string>

namespace {

Clay_String ClayStringFromStd(const std::string& value)
{
	return Clay_String{ false, static_cast<int32_t>(value.size()), value.c_str() };
}

}  // namespace

namespace silencer::client_ui::options_display {

bool IsOptionsDisplayComponent(const std::string& component)
{
	return component == kComponentFullscreenIndicator ||
	       component == kComponentSmoothScalingIndicator;
}

void ApplyOptionsDisplayRuntimeHandlers(UiDocumentRendererOptions& options)
{
	options.canBuildComponent = IsOptionsDisplayComponent;
	options.buildComponent = [](const silencer::ui::UiEditorNode& node) {
		Config & cfg = Config::GetInstance();
		bool selected = false;
		if(node.component == kComponentFullscreenIndicator){
			selected = cfg.fullscreen;
		}else if(node.component == kComponentSmoothScalingIndicator){
			selected = cfg.scalefilter;
		}else{
			return false;
		}
		const std::string indicatorId = node.id + "Content";
		silencer::client_ui::options::BooleanSettingIndicator(
			ClayStringFromStd(indicatorId),
			selected);
		return true;
	};
}

}  // namespace silencer::client_ui::options_display

namespace silencer::client_ui::options_audio {

bool IsOptionsAudioComponent(const std::string& component)
{
	return component == kComponentMusicIndicator;
}

void ApplyOptionsAudioRuntimeHandlers(UiDocumentRendererOptions& options)
{
	options.canBuildComponent = IsOptionsAudioComponent;
	options.buildComponent = [](const silencer::ui::UiEditorNode& node) {
		if(node.component != kComponentMusicIndicator){
			return false;
		}
		const std::string indicatorId = node.id + "Content";
		silencer::client_ui::options::BooleanSettingIndicator(
			ClayStringFromStd(indicatorId),
			Config::GetInstance().music);
		return true;
	};
}

}  // namespace silencer::client_ui::options_audio
