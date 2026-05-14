#include "client/ui/ClientUiInput.h"

#include <cstdlib>
#include <utility>

namespace silencer {
namespace client_ui {

void ClientUiInput::WindowToSurface(float windowX,
                                    float windowY,
                                    int windowW,
                                    int windowH,
                                    int surfaceW,
                                    int surfaceH,
                                    float& surfaceX,
                                    float& surfaceY) {
	surfaceX = windowX;
	surfaceY = windowY;
	if(windowW > 0 && windowH > 0 && surfaceW > 0 && surfaceH > 0){
		surfaceX = (windowX / static_cast<float>(windowW)) * static_cast<float>(surfaceW);
		surfaceY = (windowY / static_cast<float>(windowH)) * static_cast<float>(surfaceH);
	}
}

void ClientUiInput::AddWheelDelta(float x, float y) {
	wheelX_ += x;
	wheelY_ += y;
}

void ClientUiInput::QueueTextInput(char ascii) {
	if(ascii >= 0x20 && ascii <= 0x7E) textInput_.push_back(ascii);
}

void ClientUiInput::QueueNavAction(silencer::ui::UiNavAction action) {
	navActions_.push_back(action);
}

void ClientUiInput::QueueBindingInput(silencer::ui::UiBindingInput input) {
	bindingInputs_.push_back(input);
}

void ClientUiInput::QueueBindingKeyDown(int keyCode) {
	silencer::ui::UiBindingInput input;
	input.kind = silencer::ui::UiBindingInputKind::KeyboardKeyDown;
	input.code = keyCode;
	input.axisDir = 0;
	QueueBindingInput(input);
}

void ClientUiInput::QueueAutomationAction(silencer::ui::UiAction action) {
	silencer::ui::UiAutomationCommand command;
	command.kind = silencer::ui::UiAutomationCommandKind::Action;
	command.action = std::move(action);
	automationCommands_.push_back(std::move(command));
}

void ClientUiInput::QueueAutomationInvokeAt(int x, int y) {
	silencer::ui::UiAutomationCommand command;
	command.kind = silencer::ui::UiAutomationCommandKind::InvokeAt;
	command.x = x;
	command.y = y;
	automationCommands_.push_back(command);
}

void ClientUiInput::QueuePointerWindowEvent(float windowX,
                                            float windowY,
                                            int windowW,
                                            int windowH,
                                            int surfaceW,
                                            int surfaceH,
                                            bool pressed,
                                            bool released) {
	float sx = windowX;
	float sy = windowY;
	WindowToSurface(windowX, windowY, windowW, windowH, surfaceW, surfaceH, sx, sy);
	QueuePointerSurfaceEvent(sx, sy, pressed, released);
}

void ClientUiInput::QueuePointerSurfaceEvent(float surfaceX,
                                             float surfaceY,
                                             bool pressed,
                                             bool released) {
	pointerX_ = surfaceX;
	pointerY_ = surfaceY;
	havePointerPosition_ = true;
	if(pressed){
		pointerPressed_ = true;
		pointerDown_ = true;
	}
	if(released){
		pointerReleased_ = true;
		pointerDown_ = false;
	}
}

void ClientUiInput::SetPolledWindowPointer(float windowX,
                                           float windowY,
                                           bool down,
                                           int windowW,
                                           int windowH,
                                           int surfaceW,
                                           int surfaceH) {
	float sx = windowX;
	float sy = windowY;
	WindowToSurface(windowX, windowY, windowW, windowH, surfaceW, surfaceH, sx, sy);
	SetPolledSurfacePointer(sx, sy, down);
}

void ClientUiInput::SetPolledSurfacePointer(float surfaceX, float surfaceY, bool down) {
	pointerX_ = surfaceX;
	pointerY_ = surfaceY;
	havePointerPosition_ = true;
	pointerDown_ = down;
}

void ClientUiInput::CaptureGamepadBindingEdges(uint32_t buttons,
                                               const int16_t * axes,
                                               int axisCount,
                                               int16_t axisDeadzone) {
	if(axisCount < 0) axisCount = 0;
	if(!gamepadBindingInitialized_){
		gamepadBindingInitialized_ = true;
		gamepadBindingButtons_ = buttons;
		gamepadBindingAxes_.assign(axisCount, 0);
		for(int axis = 0; axis < axisCount; ++axis){
			gamepadBindingAxes_[axis] = axes ? axes[axis] : 0;
		}
		return;
	}

	for(int b = 0; b < 32; ++b){
		bool was = (gamepadBindingButtons_ >> b) & 1u;
		bool now = (buttons >> b) & 1u;
		if(now && !was){
			silencer::ui::UiBindingInput input;
			input.kind = silencer::ui::UiBindingInputKind::GamepadButtonDown;
			input.code = b;
			input.axisDir = 0;
			QueueBindingInput(input);
		}
	}

	if(static_cast<int>(gamepadBindingAxes_.size()) != axisCount){
		gamepadBindingAxes_.assign(axisCount, 0);
	}
	for(int axis = 0; axis < axisCount; ++axis){
		int16_t was = gamepadBindingAxes_[axis];
		int16_t now = axes ? axes[axis] : 0;
		bool wasPressed = std::abs(static_cast<int>(was)) > axisDeadzone;
		bool nowPressed = std::abs(static_cast<int>(now)) > axisDeadzone;
		if(nowPressed && !wasPressed){
			silencer::ui::UiBindingInput input;
			input.kind = silencer::ui::UiBindingInputKind::GamepadAxisMoved;
			input.code = axis;
			input.axisDir = (now > 0) ? 1 : -1;
			QueueBindingInput(input);
		}
		gamepadBindingAxes_[axis] = now;
	}
	gamepadBindingButtons_ = buttons;
}

silencer::ui::UiInputState ClientUiInput::BuildFrame(int width,
                                                     int height,
                                                     float deltaTimeSeconds) {
	silencer::ui::UiInputState input;
	input.width = width;
	input.height = height;
	input.deltaTimeSeconds = deltaTimeSeconds > 0.0f ? deltaTimeSeconds : 1.0f / 60.0f;
	input.pointer.x = havePointerPosition_ ? pointerX_ : 0.0f;
	input.pointer.y = havePointerPosition_ ? pointerY_ : 0.0f;
	input.pointer.down = pointerDown_ || pointerPressed_;
	input.pointer.pressed = pointerPressed_ || (input.pointer.down && !pointerWasDown_);
	input.pointer.released = pointerReleased_ || (!input.pointer.down && pointerWasDown_);
	input.pointer.wheelX = wheelX_;
	input.pointer.wheelY = wheelY_;
	input.textInput = textInput_;
	input.navActions = navActions_;
	input.bindingInputs = bindingInputs_;
	input.automationCommands = automationCommands_;
	lastFramePointerDown_ = input.pointer.down;
	return input;
}

void ClientUiInput::EndFrame() {
	pointerWasDown_ = lastFramePointerDown_;
	wheelX_ = 0.0f;
	wheelY_ = 0.0f;
	textInput_.clear();
	navActions_.clear();
	bindingInputs_.clear();
	automationCommands_.clear();
	pointerPressed_ = false;
	pointerReleased_ = false;
}

}  // namespace client_ui
}  // namespace silencer
