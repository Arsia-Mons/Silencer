#include "client/ui/ClayBridgeFrameBackend.h"

#include "clay_ui_compositor.h"

namespace silencer {
namespace client_ui {

ClayBridgeFrameBackend::ClayBridgeFrameBackend(bool isolated)
	: isolated_(isolated),
	  isolatedContext_(isolated
		  ? std::unique_ptr<silencer::clay_bridge::IsolatedContext>(
			  new silencer::clay_bridge::IsolatedContext())
		  : nullptr) {
}

ClayBridgeFrameBackend::~ClayBridgeFrameBackend() {
	RestorePrimaryContext();
}

void ClayBridgeFrameBackend::SetCurrentContext() {
	if(width_ > 0 && height_ > 0) {
		if(isolated_){
			isolatedContext_->SetCurrent(width_, height_);
		}else{
			silencer::clay_bridge::EnsureInitialized(width_, height_);
		}
	}
}

void ClayBridgeFrameBackend::SetLayoutDimensions(int width, int height) {
	width_ = width;
	height_ = height;
	if(isolated_){
		isolatedContext_->SetCurrent(width, height);
	}else{
		silencer::clay_bridge::EnsureInitialized(width, height);
	}
}

void ClayBridgeFrameBackend::SetUiScale(float scale) {
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

void ClayBridgeFrameBackend::RestorePrimaryContext() {
	if(isolated_){
		silencer::clay_bridge::RestorePrimaryContext();
	}
}

}  // namespace client_ui
}  // namespace silencer
