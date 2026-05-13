#include "runtime/ClayService.h"

namespace silencer {
namespace ui {

ClayService::ClayService(ClayFrameBackend& backend) : backend_(backend) {}

void ClayService::BeginFrame(const UiInputState& input, UiAutomationRegistry& automation) {
	frame_.input = input;
	frame_.automation = &automation;
	automation.BeginFrame();

	backend_.SetCurrentContext();
	backend_.SetLayoutDimensions(input.width, input.height);
	backend_.SetPointerState(input.pointer.x, input.pointer.y, input.pointer.down);
	backend_.UpdateScrollContainers(input.pointer.wheelX, input.pointer.wheelY, input.deltaTimeSeconds);
	backend_.BeginLayout();
	inFrame_ = true;
}

std::vector<UiRenderCommand> ClayService::EndFrame() {
	if(!inFrame_) return std::vector<UiRenderCommand>();
	inFrame_ = false;
	return backend_.EndLayout();
}

}  // namespace ui
}  // namespace silencer
