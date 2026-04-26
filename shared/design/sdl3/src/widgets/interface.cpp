#include "widgets/interface.h"

#include "widgets/button.h"
#include "widgets/overlay.h"

namespace silencer {

void Interface::Tick() {
    for (auto *o : overlays) o->Tick();
    for (size_t i = 0; i < buttons.size(); ++i) {
        Button *b = buttons[i];
        bool focused = (static_cast<int>(i) == activeobject);
        b->SetHovered(focused);
        b->Tick();
    }
}

}  // namespace silencer
