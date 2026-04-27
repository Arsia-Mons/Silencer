#include "panel.h"

namespace silencer {

void RenderPanel(Framebuffer &fb, const SpriteSet &sprites,
                 const PanelView &view) {
  if (!sprites.Has(view.bank, view.idx)) return;
  BlitSprite(fb, sprites.Get(view.bank, view.idx), view.x, view.y, nullptr);
}

}  // namespace silencer
