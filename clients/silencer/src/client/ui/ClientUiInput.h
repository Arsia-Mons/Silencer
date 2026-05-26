#pragma once

#include "ui/runtime/UiInputState.h"

#include <cstdint>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

class ClientUiInput {
public:
	void AddWheelDelta(float x, float y);
	void QueueTextInput(char ascii);
	void QueueNavAction(
		silencer::ui::UiNavAction action,
		silencer::ui::UiFocusSource source = silencer::ui::UiFocusSource::Keyboard);
	void QueueBindingKeyDown(int keyCode);
	void QueueControlAction(silencer::ui::UiAction action);
	void QueueControlPointerPress(int x, int y);
	void QueueControlPointerHover(int x, int y);

	void QueuePointerWindowEvent(float windowX,
	                             float windowY,
	                             int windowW,
	                             int windowH,
	                             int surfaceW,
	                             int surfaceH,
	                             bool pressed,
	                             bool released);
	void QueuePointerSurfaceEvent(float surfaceX, float surfaceY, bool pressed, bool released);
	void SetPolledWindowPointer(float windowX,
	                            float windowY,
	                            bool down,
	                            int windowW,
	                            int windowH,
	                            int surfaceW,
	                            int surfaceH);
	void SetPolledSurfacePointer(float surfaceX, float surfaceY, bool down);
	void CaptureGamepadBindingEdges(uint32_t buttons,
	                                const int16_t * axes,
	                                int axisCount,
	                                int16_t axisDeadzone);

	silencer::ui::UiInputState BuildFrame(int width, int height, float uiScale, float deltaTimeSeconds);
	void EndFrame();

private:
	static void WindowToSurface(float windowX,
	                            float windowY,
	                            int windowW,
	                            int windowH,
	                            int surfaceW,
	                            int surfaceH,
	                            float& surfaceX,
	                            float& surfaceY);
	void SetInputSource(silencer::ui::UiFocusSource source);
	void QueueBindingInput(silencer::ui::UiBindingInput input);

	float wheelX_ = 0.0f;
	float wheelY_ = 0.0f;
	std::string textInput_;
	std::vector<silencer::ui::UiNavAction> navActions_;
	std::vector<silencer::ui::UiBindingInput> bindingInputs_;
	std::vector<silencer::ui::UiControlCommand> controlCommands_;
	silencer::ui::UiFocusSource source_ = silencer::ui::UiFocusSource::None;

	bool havePointerPosition_ = false;
	bool controlPointerActive_ = false;
	float pointerX_ = 0.0f;
	float pointerY_ = 0.0f;
	bool pointerDown_ = false;
	bool pointerPressed_ = false;
	bool pointerReleased_ = false;
	bool pointerWasDown_ = false;
	bool lastFramePointerDown_ = false;

	bool gamepadBindingInitialized_ = false;
	uint32_t gamepadBindingButtons_ = 0;
	std::vector<int16_t> gamepadBindingAxes_;
};

}  // namespace client_ui
}  // namespace silencer
