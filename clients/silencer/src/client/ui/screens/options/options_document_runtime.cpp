#include "options_document_runtime.h"

#include "components/boolean_setting_row.h"
#include "controls_keybind_list.h"
#include "config.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace {

Clay_String ClayStringFromStd(const std::string& value)
{
	return Clay_String{ false, static_cast<int32_t>(value.size()), value.c_str() };
}

bool StartsWith(const std::string& value, const char * prefix) {
	const std::size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string& value, const char * prefix) {
	if(!StartsWith(value, prefix)) return -1;
	const char * suffix = value.c_str() + std::strlen(prefix);
	if(*suffix == '\0') return -1;
	if(*suffix < '0' || *suffix > '9') return -1;
	char * end = nullptr;
	const long parsed = std::strtol(suffix, &end, 10);
	if(*end != '\0' || parsed < 0 ||
	   parsed > std::numeric_limits<int>::max()){
		return -1;
	}
	return static_cast<int>(parsed);
}

}  // namespace

namespace silencer::client_ui::options_menu {

namespace {

struct OptionsMenuActionDescriptor {
	const char * token;
	OptionsMenuAction action;
};

const OptionsMenuActionDescriptor kOptionsMenuActions[] = {
	{ kActionControls, OptionsMenuAction::Controls },
	{ kActionDisplay, OptionsMenuAction::Display },
	{ kActionAudio, OptionsMenuAction::Audio },
	{ kActionBack, OptionsMenuAction::Back },
};

const char * const kOptionsMenuActionTokens[] = {
	kActionControls,
	kActionDisplay,
	kActionAudio,
	kActionBack,
};

const OptionsMenuActionDescriptor * FindOptionsMenuAction(
	const std::string& action) {
	for(const auto& descriptor : kOptionsMenuActions){
		if(action == descriptor.token) return &descriptor;
	}
	return nullptr;
}

}  // namespace

bool IsOptionsMenuAction(const std::string& action)
{
	return FindOptionsMenuAction(action) != nullptr;
}

bool HandleOptionsMenuAction(const std::string& action,
                             const OptionsMenuActionHandler& handler)
{
	const OptionsMenuActionDescriptor * descriptor =
		FindOptionsMenuAction(action);
	return descriptor && handler && handler(descriptor->action);
}

void ApplyOptionsMenuRuntimeHandlers(UiDocumentRendererOptions& options)
{
	options.canHandleAction = IsOptionsMenuAction;
	options.runtimeActions = kOptionsMenuActionTokens;
	options.runtimeActionCount = 4;
}

}  // namespace silencer::client_ui::options_menu

namespace silencer::client_ui::options_controls {

namespace {

struct OptionsControlsStaticActionDescriptor {
	const char * token;
	OptionsControlsAction::Kind kind;
};

const OptionsControlsStaticActionDescriptor kOptionsControlsStaticActions[] = {
	{ kActionPreset, OptionsControlsAction::Kind::Preset },
	{ kActionSave, OptionsControlsAction::Kind::Save },
	{ kActionCancel, OptionsControlsAction::Kind::Cancel },
};

const char * const kOptionsControlsComponents[] = { kComponentKeybindRows };
const char * const kOptionsControlsTextBindings[] = { kPresetLabelBinding };
const char * const kOptionsControlsActionTokens[] = {
	kActionPreset,
	kActionSave,
	kActionCancel,
};

const OptionsControlsStaticActionDescriptor * FindOptionsControlsStaticAction(
	const std::string& action) {
	for(const auto& descriptor : kOptionsControlsStaticActions){
		if(action == descriptor.token) return &descriptor;
	}
	return nullptr;
}

bool HandleRowAction(const std::string& action,
                     const char * prefix,
                     OptionsControlsAction::Kind kind,
                     const OptionsControlsActionHandler& handler) {
	const int row = SuffixInt(action, prefix);
	if(row < 0) return false;
	return handler && handler(OptionsControlsAction{ kind, row });
}

options::KeybindListView MakeOptionsControlsPreviewKeybindListView() {
	options::KeybindListView view;
	view.presetText = "Default";
	view.contentWidth = options::kKeybindRowsDefaultWidth;
	view.viewportHeight = options::kKeybindRowsDefaultHeight;
	view.visibleRowCount = options::kKeybindListDefaultVisibleRows;
	view.rows = {
		options::KeybindRowView{ "Move Up:", "Up", "", "OR", false, false },
		options::KeybindRowView{ "Move Down:", "Down", "", "OR", false, false },
		options::KeybindRowView{ "Move Left:", "Left", "", "OR", false, false },
		options::KeybindRowView{ "Move Right:", "Right", "", "OR", false, false },
	};
	return view;
}

const options::KeybindListView& OptionsControlsPreviewKeybindListView() {
	static const options::KeybindListView view =
		MakeOptionsControlsPreviewKeybindListView();
	return view;
}

}  // namespace

bool IsOptionsControlsComponent(const std::string& component)
{
	return component == kComponentKeybindRows;
}

bool IsOptionsControlsTextBinding(const std::string& binding)
{
	return binding == kPresetLabelBinding;
}

bool IsOptionsControlsAction(const std::string& action)
{
	return HandleOptionsControlsAction(
		action,
		[](const OptionsControlsAction&) { return true; });
}

bool HandleOptionsControlsAction(const std::string& action,
                                 const OptionsControlsActionHandler& handler)
{
	const OptionsControlsStaticActionDescriptor * descriptor =
		FindOptionsControlsStaticAction(action);
	if(descriptor){
		return handler && handler(OptionsControlsAction{ descriptor->kind, -1 });
	}
	if(HandleRowAction(action, kActionPrimaryPrefix,
	                   OptionsControlsAction::Kind::Primary, handler)){
		return true;
	}
	if(HandleRowAction(action, kActionSecondaryPrefix,
	                   OptionsControlsAction::Kind::Secondary, handler)){
		return true;
	}
	return HandleRowAction(action, kActionOperatorPrefix,
	                       OptionsControlsAction::Kind::Operator, handler);
}

OptionsControlsRuntimeContext OptionsControlsPreviewRuntimeContext()
{
	return OptionsControlsRuntimeContext{ &OptionsControlsPreviewKeybindListView() };
}

OptionsControlsRuntimeContext OptionsControlsLiveRuntimeContext(
	const options::KeybindListView& keybindListView)
{
	return OptionsControlsRuntimeContext{ &keybindListView };
}

void ApplyOptionsControlsRuntimeHandlers(
	UiDocumentRendererOptions& options,
	const OptionsControlsRuntimeContext& context)
{
	const options::KeybindListView * keybindListView = context.keybindListView;
	options.runtimeComponents = kOptionsControlsComponents;
	options.runtimeComponentCount = 1;
	options.runtimeTextBindings = kOptionsControlsTextBindings;
	options.runtimeTextBindingCount = 1;
	options.runtimeActions = kOptionsControlsActionTokens;
	options.runtimeActionCount = 3;
	options.canBuildComponent = [keybindListView](const std::string& component) {
		return keybindListView && IsOptionsControlsComponent(component);
	};
	options.canResolveTextBinding = IsOptionsControlsTextBinding;
	options.canHandleAction = IsOptionsControlsAction;
	options.resolveTextBinding = [keybindListView](const std::string& binding,
	                                               std::string& out) {
		if(!IsOptionsControlsTextBinding(binding)) return false;
		out = keybindListView && !keybindListView->presetText.empty()
			? keybindListView->presetText
			: std::string("default");
		return true;
	};
	options.buildComponent = [keybindListView](
		const silencer::ui::UiEditorNode& node,
		silencer::ui::UiInteractionRegistry& interactions) {
		if(!keybindListView || node.component != kComponentKeybindRows) return false;
		silencer::client_ui::options::BuildKeybindRows(
			*keybindListView,
			interactions);
		return true;
	};
}

}  // namespace silencer::client_ui::options_controls

namespace silencer::client_ui::options_display {

namespace {

struct OptionsDisplayActionDescriptor {
	const char * token;
	OptionsDisplayAction action;
};

const OptionsDisplayActionDescriptor kOptionsDisplayActions[] = {
	{ kActionFullscreen, OptionsDisplayAction::Fullscreen },
	{ kActionSmoothScaling, OptionsDisplayAction::SmoothScaling },
	{ kActionSave, OptionsDisplayAction::Save },
	{ kActionCancel, OptionsDisplayAction::Cancel },
};

const char * const kOptionsDisplayComponents[] = {
	kComponentFullscreenIndicator,
	kComponentSmoothScalingIndicator,
};
const char * const kOptionsDisplayActionTokens[] = {
	kActionFullscreen,
	kActionSmoothScaling,
	kActionSave,
	kActionCancel,
};

const OptionsDisplayActionDescriptor * FindOptionsDisplayAction(
	const std::string& action) {
	for(const auto& descriptor : kOptionsDisplayActions){
		if(action == descriptor.token) return &descriptor;
	}
	return nullptr;
}

}  // namespace

bool IsOptionsDisplayComponent(const std::string& component)
{
	return component == kComponentFullscreenIndicator ||
	       component == kComponentSmoothScalingIndicator;
}

bool IsOptionsDisplayAction(const std::string& action)
{
	return FindOptionsDisplayAction(action) != nullptr;
}

bool HandleOptionsDisplayAction(const std::string& action,
                                const OptionsDisplayActionHandler& handler)
{
	const OptionsDisplayActionDescriptor * descriptor =
		FindOptionsDisplayAction(action);
	return descriptor && handler && handler(descriptor->action);
}

void ApplyOptionsDisplayRuntimeHandlers(UiDocumentRendererOptions& options)
{
	options.runtimeComponents = kOptionsDisplayComponents;
	options.runtimeComponentCount = 2;
	options.runtimeActions = kOptionsDisplayActionTokens;
	options.runtimeActionCount = 4;
	options.canBuildComponent = IsOptionsDisplayComponent;
	options.canHandleAction = IsOptionsDisplayAction;
	options.buildComponent = [](const silencer::ui::UiEditorNode& node,
	                            silencer::ui::UiInteractionRegistry& interactions) {
		(void)interactions;
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

namespace {

struct OptionsAudioActionDescriptor {
	const char * token;
	OptionsAudioAction action;
};

const OptionsAudioActionDescriptor kOptionsAudioActions[] = {
	{ kActionMusic, OptionsAudioAction::Music },
	{ kActionSave, OptionsAudioAction::Save },
	{ kActionCancel, OptionsAudioAction::Cancel },
};

const char * const kOptionsAudioComponents[] = { kComponentMusicIndicator };
const char * const kOptionsAudioActionTokens[] = {
	kActionMusic,
	kActionSave,
	kActionCancel,
};

const OptionsAudioActionDescriptor * FindOptionsAudioAction(
	const std::string& action) {
	for(const auto& descriptor : kOptionsAudioActions){
		if(action == descriptor.token) return &descriptor;
	}
	return nullptr;
}

}  // namespace

bool IsOptionsAudioComponent(const std::string& component)
{
	return component == kComponentMusicIndicator;
}

bool IsOptionsAudioAction(const std::string& action)
{
	return FindOptionsAudioAction(action) != nullptr;
}

bool HandleOptionsAudioAction(const std::string& action,
                              const OptionsAudioActionHandler& handler)
{
	const OptionsAudioActionDescriptor * descriptor =
		FindOptionsAudioAction(action);
	return descriptor && handler && handler(descriptor->action);
}

void ApplyOptionsAudioRuntimeHandlers(UiDocumentRendererOptions& options)
{
	options.runtimeComponents = kOptionsAudioComponents;
	options.runtimeComponentCount = 1;
	options.runtimeActions = kOptionsAudioActionTokens;
	options.runtimeActionCount = 3;
	options.canBuildComponent = IsOptionsAudioComponent;
	options.canHandleAction = IsOptionsAudioAction;
	options.buildComponent = [](const silencer::ui::UiEditorNode& node,
	                            silencer::ui::UiInteractionRegistry& interactions) {
		(void)interactions;
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
