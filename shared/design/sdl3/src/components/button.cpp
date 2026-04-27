#include "button.h"

#include "../font.h"

namespace silencer {

namespace {

struct Variant {
  int chrome_bank;     // -1 = no chrome (text-only)
  int chrome_idx;
  int width;
  int label_bank;
  int label_advance;
  int label_yoff;
  int xoff_nudge;      // extra px added to centering xoff (B52x21 only)
};

const Variant &Lookup(ButtonVariant v) {
  static const Variant kB196x33 = {6,  7, 196, 135, 11, 8, 0};
  static const Variant kB220x33 = {6, 23, 220, 135, 11, 8, 0};
  static const Variant kB112x33 = {6, 28, 112, 135, 11, 8, 0};
  static const Variant kB156x21 = {7, 24, 156, 134,  8, 4, 0};
  static const Variant kB52x21  = {-1, 0,  52, 133,  7, 8, 1};
  switch (v) {
    case ButtonVariant::B196x33: return kB196x33;
    case ButtonVariant::B220x33: return kB220x33;
    case ButtonVariant::B112x33: return kB112x33;
    case ButtonVariant::B156x21: return kB156x21;
    case ButtonVariant::B52x21:  return kB52x21;
  }
  return kB196x33;  // unreachable
}

}  // namespace

void RenderButton(Framebuffer &fb, const SpriteSet &sprites,
                  const Palette &palette, int active_sub,
                  const ButtonView &view) {
  const Variant &v = Lookup(view.variant);

  int len = static_cast<int>(view.label.size());
  int xoff = (v.width - len * v.label_advance) / 2 + v.xoff_nudge;

  int text_x;
  int text_y;
  if (v.chrome_bank >= 0 && sprites.Has(v.chrome_bank, v.chrome_idx)) {
    const Sprite &chrome = sprites.Get(v.chrome_bank, v.chrome_idx);
    BlitSprite(fb, chrome, view.x, view.y, nullptr);
    text_x = view.x - chrome.offset_x + xoff;
    text_y = view.y - chrome.offset_y + v.label_yoff;
  } else {
    // Text-only: anchor IS top-left.
    text_x = view.x + xoff;
    text_y = view.y + v.label_yoff;
  }

  if (!view.label.empty()) {
    DrawText(fb, text_x, text_y, view.label, v.label_bank, v.label_advance,
             sprites, palette, active_sub, view.brightness);
  }
}

}  // namespace silencer
