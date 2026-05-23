#ifndef SILENCER_CLIENT_UI_CLAY_BRIDGE_FRAME_BACKEND_H
#define SILENCER_CLIENT_UI_CLAY_BRIDGE_FRAME_BACKEND_H

#include "clay/clay.h"
#include "ui/runtime/ClayService.h"
#include <memory>

namespace silencer {
namespace clay_bridge {
class IsolatedContext;
}
}

namespace silencer {
namespace client_ui {

class ClayBridgeFrameBackend : public silencer::ui::ClayFrameBackend {
public:
	explicit ClayBridgeFrameBackend(bool isolated = false);
	~ClayBridgeFrameBackend();

	void SetCurrentContext() override;
	void SetLayoutDimensions(int width, int height) override;
	void SetUiScale(float scale) override;
	void SetPointerState(float x, float y, bool down) override;
	void UpdateScrollContainers(float wheelX, float wheelY, float deltaTimeSeconds) override;
	void BeginLayout() override;
	std::vector<silencer::ui::UiRenderCommand> EndLayout() override;
	void RestorePreviousContext();

	Clay_RenderCommandArray Commands() const { return commands_; }

private:
	Clay_RenderCommandArray commands_{};
	int width_ = 0;
	int height_ = 0;
	bool isolated_ = false;
	std::unique_ptr<silencer::clay_bridge::IsolatedContext> isolatedContext_;
};

}  // namespace client_ui
}  // namespace silencer

#endif
