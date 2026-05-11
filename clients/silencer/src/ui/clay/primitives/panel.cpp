#include "panel.h"

#include "clay_bridge.h"

namespace silencer::ui::primitives {

void Panel(Clay_String id,
           PanelVariant variant,
           PanelOpts opts) {
	const float w = static_cast<float>(opts.width);
	const float h = static_cast<float>(opts.height);

	void * imageData = nullptr;
	if(variant == PanelVariant::RightChrome){
		imageData = silencer::clay_bridge::PackImage(
			opts.chromeBank, opts.chromeIndex);
	}

	// Outer panel block. Padding shifts the title relative to the panel's
	// top-left without affecting where the chrome IMAGE is blitted (the
	// bridge always draws IMAGE at the bbox top-left). Title is a regular
	// child of this block — Clay emits the IMAGE render command for the
	// parent first, so any title text overlays the chrome.
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h) },
	           .padding = { /*left=*/opts.titlePadLeft,
	                        /*right=*/0,
	                        /*top=*/opts.titlePadTop,
	                        /*bottom=*/0 },
	       },
	       .image = { .imageData = imageData } }) {
		if(opts.title.length > 0){
			BankText(opts.title, opts.titleVariant,
			         { .effectColor = opts.titleEffectColor,
			           .brightness  = opts.titleBrightness });
		}
	}
}

}  // namespace silencer::ui::primitives
