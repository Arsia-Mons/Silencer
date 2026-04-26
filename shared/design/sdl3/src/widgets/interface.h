#pragma once
#include <cstdint>
#include <vector>

namespace silencer {

struct Overlay;
struct Button;

// A non-rendering container. Holds the menu's drawable children in
// draw order, plus a focus index into the button list.
struct Interface {
    std::vector<Overlay *> overlays;  // in draw order
    std::vector<Button *> buttons;    // also drawn after overlays
    int activeobject = -1;            // index into buttons; -1 = none
    int buttonenter = -1;
    int buttonescape = -1;

    void Tick();
};

}  // namespace silencer
