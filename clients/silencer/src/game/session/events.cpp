#include "game.h"

#include "audio.h"
#include "config.h"
#include "ui/input.h"
#include "world.h"
#include <cstring>

namespace {
// SDL keycode -> the runtime's text-editing UiKey vocabulary. Printable
// characters arrive as SDL_EVENT_TEXT_INPUT, not as key events.
::ui::UiKey UiKeyFromSdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_BACKSPACE:
        return ::ui::UiKey::Backspace;
    case SDLK_DELETE:
        return ::ui::UiKey::DeleteForward;
    case SDLK_LEFT:
        return ::ui::UiKey::Left;
    case SDLK_RIGHT:
        return ::ui::UiKey::Right;
    case SDLK_HOME:
        return ::ui::UiKey::Home;
    case SDLK_END:
        return ::ui::UiKey::End;
    case SDLK_UP:
        return ::ui::UiKey::Up;
    case SDLK_DOWN:
        return ::ui::UiKey::Down;
    case SDLK_PAGEUP:
        return ::ui::UiKey::PageUp;
    case SDLK_PAGEDOWN:
        return ::ui::UiKey::PageDown;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return ::ui::UiKey::Enter;
    case SDLK_TAB:
        return ::ui::UiKey::Tab;
    case SDLK_A:
        return ::ui::UiKey::A;
    default:
        return ::ui::UiKey::Unknown;
    }
}

uint16_t UiModsFromSdl(SDL_Keymod mod) {
    uint16_t out = ::ui::UI_KEY_MOD_NONE;
    if (mod & SDL_KMOD_SHIFT)
        out |= ::ui::UI_KEY_MOD_SHIFT;
    if (mod & SDL_KMOD_CTRL)
        out |= ::ui::UI_KEY_MOD_CTRL;
    if (mod & SDL_KMOD_ALT)
        out |= ::ui::UI_KEY_MOD_ALT;
    if (mod & SDL_KMOD_GUI)
        out |= ::ui::UI_KEY_MOD_SUPER;
    return out;
}

// Does this gamepad button resolve to the keymap's UiCancel binding? Asks the
// keymap so gamepad.json's "ui_cancel" stays authoritative (no hardcoded button).
bool GamepadButtonIsUiCancel(const KeyMap &keymap, int button) {
    GamepadState gp = {};
    gp.connected = true;
    if (button >= 0 && button < 32)
        gp.buttons = (1u << (unsigned)button);
    return keymap.IsPressed(Action::UiCancel, nullptr, gp);
}
} // namespace

bool Game::HandleSDLEvents() {
    if (world.dedicatedserver.active) {
        return true;
    }
    if (headless || tui) {
        return true;
    }
    SDL_Event event;
    while (SDL_PollEvent(&event) > 0) {
        switch (event.type) {
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            SyncRenderSurfaceToWindowPixels();
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (!world.replay.IsPlaying())
                Audio::GetInstance().Unmute();
            minimized = false;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (!world.replay.IsPlaying())
                Audio::GetInstance().Mute(25);
            minimized = true;
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            minimized = true;
            break;
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            minimized = false;
            break;
        case SDL_EVENT_KEY_DOWN: {
            gameInput.OnScancodeDown(event.key.scancode);
            gameInput.GetKeystate()[event.key.scancode] = true;
            // While rebinding, keyboard edges build the pending chord instead of
            // driving UI nav/confirm, so Escape/Enter/arrows are themselves bindable.
            if (gameUiPipeline.IsCapturingKeybind()) {
                gameUiPipeline.FeedKeybindEdge({BindingDevice::Keyboard,
                                                (int)event.key.scancode, 0});
                break;
            }
            ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
            ui.source = ::ui::UiFocusSource::Keyboard;
            ::ui::UiKey k = UiKeyFromSdl(event.key.key);
            uint16_t mods = UiModsFromSdl(SDL_GetModState());
            if (k != ::ui::UiKey::Unknown) {
                ::ui::ui_input_push_key(ui, k, mods, event.key.repeat);
            }
            switch (event.key.key) {
            case SDLK_UP:
                ui.nav_up = true;
                break;
            case SDLK_DOWN:
                ui.nav_down = true;
                break;
            case SDLK_LEFT:
                ui.nav_left = true;
                break;
            case SDLK_RIGHT:
                ui.nav_right = true;
                break;
            case SDLK_TAB:
                if (mods & ::ui::UI_KEY_MOD_SHIFT)
                    ui.nav_previous = true;
                else
                    ui.nav_next = true;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                ui.confirm_pressed = true;
                ui.confirm_down = true;
                break;
            // SPACE confirms a button, but in a focused text field it types a
            // space (via TEXT_INPUT) instead of submitting. Only Enter submits.
            case SDLK_SPACE:
                if (!gameUiPipeline.WantsTextInput()) {
                    ui.confirm_pressed = true;
                    ui.confirm_down = true;
                }
                break;
            case SDLK_ESCAPE:
                ui.cancel_pressed = true;
                ui.cancel_down = true;
                break;
            default:
                break;
            }
        } break;
        case SDL_EVENT_KEY_UP: {
            gameInput.OnScancodeUp(event.key.scancode);
            gameInput.GetKeystate()[event.key.scancode] = false;
            ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
            switch (event.key.key) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                ui.confirm_released = true;
                break;
            case SDLK_ESCAPE:
                ui.cancel_released = true;
                break;
            default:
                break;
            }
        } break;
        case SDL_EVENT_MOUSE_WHEEL: {
            // SDL reports +y = wheel up; FLIPPED (natural-scroll) inverts both axes.
            if (gameUiPipeline.IsCapturingKeybind())
                break;
            ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
            float dir = (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -1.0f : 1.0f;
            ui.wheel_x += dir * event.wheel.x;
            ui.wheel_y += dir * event.wheel.y;
            ui.source = ::ui::UiFocusSource::Mouse;
        } break;
        case SDL_EVENT_TEXT_INPUT: {
            if (gameUiPipeline.IsCapturingKeybind())
                break;
            ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
            ::ui::ui_input_push_text(ui, event.text.text);
            ui.source = ::ui::UiFocusSource::Keyboard;
        } break;
        case SDL_EVENT_TEXT_EDITING: {
            if (gameUiPipeline.IsCapturingKeybind())
                break;
            ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
            ::ui::ui_input_push_editing(ui, event.edit.text, event.edit.start,
                                        event.edit.length);
            ui.source = ::ui::UiFocusSource::Keyboard;
        } break;
        // Keybind-capture edges. Mouse-left is gated to active capture so it never
        // steals a UI click; gamepad button/axis are event-driven only while capturing.
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (gameUiPipeline.IsCapturingKeybind()) {
                gameUiPipeline.FeedKeybindEdge({BindingDevice::Mouse,
                                                (int)event.button.button, 0});
            } else if (event.button.button == SDL_BUTTON_LEFT) {
                // Latch the press EDGE from the event queue (sticky for the frame)
                // so a click that begins and ends between two per-frame pointer
                // polls is never dropped when the frame rate dips. Mirrors how
                // keyboard confirm and the wheel already accumulate here.
                ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
                ui.pointer_pressed = true;
                ui.source = ::ui::UiFocusSource::Mouse;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (!gameUiPipeline.IsCapturingKeybind() &&
                event.button.button == SDL_BUTTON_LEFT) {
                ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
                ui.pointer_released = true;
                ui.source = ::ui::UiFocusSource::Mouse;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (gameUiPipeline.IsCapturingKeybind()) {
                gameUiPipeline.FeedKeybindEdge({BindingDevice::GamepadButton,
                                                (int)event.gbutton.button, 0});
            } else if (GamepadButtonIsUiCancel(gameInput.GetKeyMap(), (int)event.gbutton.button)) {
                ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
                ui.source = ::ui::UiFocusSource::Gamepad;
                ui.cancel_pressed = true;
                ui.cancel_down = true;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (!gameUiPipeline.IsCapturingKeybind() &&
                GamepadButtonIsUiCancel(gameInput.GetKeyMap(), (int)event.gbutton.button)) {
                ::ui::UiInputFrame &ui = gameUiPipeline.UiInput();
                ui.source = ::ui::UiFocusSource::Gamepad;
                ui.cancel_released = true;
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (gameUiPipeline.IsCapturingKeybind()) {
                int v = event.gaxis.value;
                if (v > AXIS_DEADZONE) {
                    gameUiPipeline.FeedKeybindEdge({BindingDevice::GamepadAxis,
                                                    (int)event.gaxis.axis, 1});
                } else if (v < -AXIS_DEADZONE) {
                    gameUiPipeline.FeedKeybindEdge({BindingDevice::GamepadAxis,
                                                    (int)event.gaxis.axis, -1});
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!gameInput.GetGamepad())
                gameInput.OpenFirstGamepad();
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (gameInput.GetGamepad() && event.gdevice.which == SDL_GetGamepadID(gameInput.GetGamepad())) {
                SDL_CloseGamepad(gameInput.GetGamepad());
                gameInput.GamepadRef() = nullptr;
                gameInput.GetGamepadStateMutable().connected = false;
                if (!gameInput.PrevGamepadProfileRef().empty()) {
                    std::strncpy(Config::GetInstance().active_keybind_profile,
                                 gameInput.PrevGamepadProfileRef().c_str(),
                                 sizeof(Config::GetInstance().active_keybind_profile) - 1);
                    Config::GetInstance().active_keybind_profile[sizeof(Config::GetInstance().active_keybind_profile) - 1] = '\0';
                    LoadActiveKeymap(gameInput.GetKeyMap());
                    gameInput.PrevGamepadProfileRef().clear();
                }
            }
            break;
        case SDL_EVENT_QUIT:
            return false;
        }
    }
    return true;
}
