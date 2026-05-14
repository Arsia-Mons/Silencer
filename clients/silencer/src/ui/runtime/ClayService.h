#pragma once

#include "runtime/UiAutomationRegistry.h"
#include "runtime/UiInputState.h"

#include <string>
#include <vector>

namespace silencer {
namespace ui {

struct UiRenderCommand {
	std::string debugName;
};

class ClayFrameBackend {
public:
	virtual ~ClayFrameBackend() = default;
	virtual void SetCurrentContext() = 0;
	virtual void SetLayoutDimensions(int width, int height) = 0;
	virtual void SetPointerState(float x, float y, bool down) = 0;
	virtual void UpdateScrollContainers(float wheelX, float wheelY, float deltaTimeSeconds) = 0;
	virtual void BeginLayout() = 0;
	virtual std::vector<UiRenderCommand> EndLayout() = 0;
};

struct ClayFrameState {
	UiInputState input;
	UiAutomationRegistry* automation = nullptr;
};

class ClayService {
public:
	explicit ClayService(ClayFrameBackend& backend);

	void BeginFrame(const UiInputState& input, UiAutomationRegistry& automation);
	std::vector<UiRenderCommand> EndFrame();
	const ClayFrameState& Frame() const { return frame_; }

private:
	ClayFrameBackend& backend_;
	ClayFrameState frame_;
	bool inFrame_ = false;
};

}  // namespace ui
}  // namespace silencer
