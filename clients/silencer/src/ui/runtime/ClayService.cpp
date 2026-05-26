#include "runtime/ClayService.h"

#include "runtime/react.h"

namespace silencer {
namespace ui {

ClayService::ClayService(ClayFrameBackend& backend) : backend_(backend) {}

ClayService::~ClayService() {
	if(reactInitialized_) react_shutdown();
}

void ClayService::BeginFrame(const UiInputState& input, UiInteractionRegistry& interactions) {
	PrepareFrame(input, interactions);
	BeginPreparedLayout();
}

void ClayService::PrepareFrame(const UiInputState& input,
                               UiInteractionRegistry& interactions) {
	frame_.input = input;
	frame_.interactions = &interactions;
	interactions.BeginFrame();

	backend_.SetCurrentContext();
	backend_.SetLayoutDimensions(input.width, input.height);
	backend_.SetUiScale(input.uiScale);
	backend_.SetPointerState(input.pointer.x, input.pointer.y, input.pointer.down);
	backend_.UpdateScrollContainers(input.pointer.wheelX, input.pointer.wheelY, input.deltaTimeSeconds);
	if(!reactInitialized_){
		react_init(Clay_GetCurrentContext());
		reactInitialized_ = true;
	}
}

void ClayService::BeginPreparedLayout() {
	react_begin_frame();
	backend_.BeginLayout();
	inFrame_ = true;
}

Clay_RenderCommandArray ClayService::EndPreparedLayout() {
	if(!inFrame_) return Clay_RenderCommandArray{};
	inFrame_ = false;
	Clay_RenderCommandArray commands = backend_.EndLayout();
	if(frame_.interactions){
		frame_.interactions->ResolveClayBoundsFromClay();
	}
	return commands;
}

void ClayService::EndPreparedFrame() {
	if(!reactInitialized_) return;
	react_end_frame();
}

Clay_RenderCommandArray ClayService::EndFrame() {
	Clay_RenderCommandArray commands = EndPreparedLayout();
	EndPreparedFrame();
	return commands;
}

}  // namespace ui
}  // namespace silencer
