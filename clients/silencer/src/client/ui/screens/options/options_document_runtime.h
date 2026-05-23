#ifndef OPTIONS_DOCUMENT_RUNTIME_H
#define OPTIONS_DOCUMENT_RUNTIME_H

#include "layout/ui_document_renderer.h"
#include "ui_layout_contract.generated.h"

#include <functional>
#include <string>

namespace silencer::client_ui::options {
struct KeybindListView;
}

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

enum class OptionsMenuAction {
	Controls,
	Display,
	Audio,
	Back,
};

using OptionsMenuActionHandler = std::function<bool(OptionsMenuAction)>;

bool IsOptionsMenuAction(const std::string& action);
bool HandleOptionsMenuAction(const std::string& action,
                             const OptionsMenuActionHandler& handler);
void ApplyOptionsMenuRuntimeHandlers(UiDocumentRendererOptions& options);

}  // namespace silencer::client_ui::options_menu

namespace silencer::client_ui::options_controls {

constexpr const char * kOptionsControlsSurface =
	silencer::net::ui_layout_contract::kUiSurfaceOptionsControls;
constexpr const char * kComponentKeybindRows =
	silencer::net::ui_layout_contract::kUiComponentOptionsControlsKeybindRows;
constexpr const char * kPresetLabelBinding =
	silencer::net::ui_layout_contract::kUiTextBindingOptionsControlsPresetLabel;
constexpr const char * kActionPreset =
	silencer::net::ui_layout_contract::kUiActionOptionsControlsPreset;
constexpr const char * kActionSave =
	silencer::net::ui_layout_contract::kUiActionOptionsControlsSave;
constexpr const char * kActionCancel =
	silencer::net::ui_layout_contract::kUiActionOptionsControlsCancel;
constexpr const char * kActionPrimaryPrefix = "options_controls.primary.";
constexpr const char * kActionSecondaryPrefix = "options_controls.secondary.";
constexpr const char * kActionOperatorPrefix = "options_controls.operator.";

struct OptionsControlsAction {
	enum class Kind {
		Preset,
		Save,
		Cancel,
		Primary,
		Secondary,
		Operator,
	};
	OptionsControlsAction(Kind kindIn, int rowIn = -1)
		: kind(kindIn), row(rowIn) {}
	Kind kind;
	int row = -1;
};

using OptionsControlsActionHandler =
	std::function<bool(const OptionsControlsAction&)>;

struct OptionsControlsRuntimeContext {
	const options::KeybindListView * keybindListView = nullptr;
};

bool IsOptionsControlsComponent(const std::string& component);
bool IsOptionsControlsTextBinding(const std::string& binding);
bool IsOptionsControlsAction(const std::string& action);
bool HandleOptionsControlsAction(const std::string& action,
                                 const OptionsControlsActionHandler& handler);
OptionsControlsRuntimeContext OptionsControlsPreviewRuntimeContext();
OptionsControlsRuntimeContext OptionsControlsLiveRuntimeContext(
	const options::KeybindListView& keybindListView);
void ApplyOptionsControlsRuntimeHandlers(
	UiDocumentRendererOptions& options,
	const OptionsControlsRuntimeContext& context);

}  // namespace silencer::client_ui::options_controls

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

enum class OptionsDisplayAction {
	Fullscreen,
	SmoothScaling,
	Save,
	Cancel,
};

using OptionsDisplayActionHandler = std::function<bool(OptionsDisplayAction)>;

bool IsOptionsDisplayComponent(const std::string& component);
bool IsOptionsDisplayAction(const std::string& action);
bool HandleOptionsDisplayAction(const std::string& action,
                                const OptionsDisplayActionHandler& handler);
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

enum class OptionsAudioAction {
	Music,
	Save,
	Cancel,
};

using OptionsAudioActionHandler = std::function<bool(OptionsAudioAction)>;

bool IsOptionsAudioComponent(const std::string& component);
bool IsOptionsAudioAction(const std::string& action);
bool HandleOptionsAudioAction(const std::string& action,
                              const OptionsAudioActionHandler& handler);
void ApplyOptionsAudioRuntimeHandlers(UiDocumentRendererOptions& options);

}  // namespace silencer::client_ui::options_audio

#endif
