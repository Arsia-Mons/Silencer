// Common widget base used by the design system demo.
//
// This is an *evaluation* hydration of the spec — not a 1:1 mirror of the
// engine's `Object`/`Sprite` class hierarchy. It carries just enough state to
// honor the shared sprite properties documented in §Shared Base.
#pragma once

#include <cstdint>
#include <string>

namespace silencer {

class SpriteBanks;
class Palette;

// Mouse state passed to widgets every frame.
struct MouseState {
    int x = 0;
    int y = 0;
    bool down = false;
    bool clicked = false;  // edge-triggered: true on the frame the button went down
    int wheel = 0;         // +1 up, -1 down
};

// Per-frame draw context.
struct DrawCtx {
    std::uint8_t* dst;
    int dst_w;
    int dst_h;
    const SpriteBanks* banks;
    const Palette* palette;
    std::uint32_t state_i;  // global UI animation tick (~24 Hz)
};

class Widget {
   public:
    virtual ~Widget() = default;

    // Position in 640x480 logical space.
    int x = 0;
    int y = 0;

    // Visibility.
    bool draw_visible = true;
    bool focused = false;

    // Optional sprite-based base.
    std::uint8_t res_bank = 0;
    std::uint8_t res_index = 0;
    std::uint8_t effect_color = 0;
    std::uint8_t effect_brightness = 128;
    bool mirrored = false;

    virtual void Tick() {}                                 // 23.8 Hz simulation tick
    virtual void Draw(const DrawCtx& ctx) = 0;             // every render frame
    virtual bool HitTest(int mx, int my) const { return false; }
    virtual void OnMouse(const MouseState&, const DrawCtx&) {}
    virtual void OnKey(int sdl_keycode) {}
    virtual void OnTextInput(const char* utf8) {}
};

}  // namespace silencer
