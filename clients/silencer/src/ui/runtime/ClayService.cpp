#include "runtime/ClayService.h"

namespace silencer {
namespace ui {

ClayService::ClayService(ClayFrameBackend& backend) : backend_(backend) {}

void ClayService::BeginFrame(const UiInputState& input, UiInteractionRegistry& interactions) {
	frame_.input = input;
	frame_.interactions = &interactions;
	interactions.BeginFrame();

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
	std::vector<UiRenderCommand> commands = backend_.EndLayout();
	if(frame_.interactions){
		frame_.interactions->ResolveClayBoundsFromClay();
	}
	return commands;
}

}  // namespace ui
}  // namespace silencer
