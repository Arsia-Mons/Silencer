#include "client/ui/ClayBridgeFrameBackend.h"

#include "clay_ui_compositor.h"

namespace silencer {
namespace client_ui {

void ClayBridgeFrameBackend::SetCurrentContext() {
	if(width_ > 0 && height_ > 0) {
		silencer::clay_bridge::EnsureInitialized(width_, height_);
	}
}

void ClayBridgeFrameBackend::SetLayoutDimensions(int width, int height) {
	width_ = width;
	height_ = height;
	silencer::clay_bridge::EnsureInitialized(width, height);
}

void ClayBridgeFrameBackend::SetUiScale(int scale) {
	silencer::clay_bridge::SetUiScale(scale);
}

void ClayBridgeFrameBackend::SetPointerState(float x, float y, bool down) {
	Clay_SetPointerState(Clay_Vector2{ x, y }, down);
}

void ClayBridgeFrameBackend::UpdateScrollContainers(float wheelX, float wheelY, float deltaTimeSeconds) {
	Clay_UpdateScrollContainers(true, Clay_Vector2{ wheelX, wheelY }, deltaTimeSeconds);
}

void ClayBridgeFrameBackend::BeginLayout() {
	Clay_ResetMeasureTextCache();
	Clay_BeginLayout();
}

std::vector<silencer::ui::UiRenderCommand> ClayBridgeFrameBackend::EndLayout() {
	commands_ = Clay_EndLayout();
	return std::vector<silencer::ui::UiRenderCommand>();
}

}  // namespace client_ui
}  // namespace silencer
