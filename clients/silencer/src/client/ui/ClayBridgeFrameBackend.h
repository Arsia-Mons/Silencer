#ifndef SILENCER_CLIENT_UI_CLAY_BRIDGE_FRAME_BACKEND_H
#define SILENCER_CLIENT_UI_CLAY_BRIDGE_FRAME_BACKEND_H

#include "clay/clay.h"
#include "ui/runtime/ClayService.h"

namespace silencer {
namespace client_ui {

class ClayBridgeFrameBackend : public silencer::ui::ClayFrameBackend {
public:
	void SetCurrentContext() override;
	void SetLayoutDimensions(int width, int height) override;
	void SetUiScale(float scale) override;
	void SetPointerState(float x, float y, bool down) override;
	void UpdateScrollContainers(float wheelX, float wheelY, float deltaTimeSeconds) override;
	void BeginLayout() override;
	std::vector<silencer::ui::UiRenderCommand> EndLayout() override;

	Clay_RenderCommandArray Commands() const { return commands_; }

private:
	Clay_RenderCommandArray commands_{};
	int width_ = 0;
	int height_ = 0;
};

}  // namespace client_ui
}  // namespace silencer

#endif
