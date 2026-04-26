#include "modal.h"

#include "../font.h"
#include "../sprite.h"

namespace silencer {

void DrawModal(std::uint8_t* dst, int dst_w, int dst_h, const std::string& message, bool ok,
               const SpriteBanks& banks, const Palette& pal) {
    // Background plate is pre-centered via baked-in sprite offsets.
    banks.Blit(dst, dst_w, dst_h, 40, 4, 320, 240);

    int y = ok ? 200 : 218;
    int len = static_cast<int>(message.size());
    int x = 320 - (len * 8) / 2;
    DrawTextOpts opts;
    opts.bank = 134;
    opts.width = 8;
    DrawText(dst, dst_w, dst_h, x, y, message, opts, banks, pal);

    // OK button is rendered by the caller (a Button widget) at (242, 230).
}

}  // namespace silencer
