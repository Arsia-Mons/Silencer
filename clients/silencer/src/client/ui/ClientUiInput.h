#pragma once

#include "ui/runtime/UiInputState.h"

#include <array>
#include <cstdint>

namespace silencer {
namespace client_ui {

constexpr int CLIENT_UI_INPUT_MAX_GAMEPAD_AXES = 16;

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
	int GamepadBindingAxisOverflowCount() const { return gamepadBindingAxisOverflowCount_; }

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
	silencer::ui::UiTextInputBuffer textInput_;
	silencer::ui::UiBoundedInputList<
		silencer::ui::UiNavAction,
		silencer::ui::UI_INPUT_MAX_NAV_ACTIONS> navActions_;
	silencer::ui::UiBoundedInputList<
		silencer::ui::UiBindingInput,
		silencer::ui::UI_INPUT_MAX_BINDING_INPUTS> bindingInputs_;
	silencer::ui::UiBoundedInputList<
		silencer::ui::UiControlCommand,
		silencer::ui::UI_INPUT_MAX_CONTROL_COMMANDS> controlCommands_;
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
	std::array<int16_t, CLIENT_UI_INPUT_MAX_GAMEPAD_AXES> gamepadBindingAxes_ = {};
	int gamepadBindingAxisCount_ = 0;
	int gamepadBindingAxisOverflowCount_ = 0;
};

}  // namespace client_ui
}  // namespace silencer
