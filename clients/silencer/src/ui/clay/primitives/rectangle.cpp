#include "primitives/rectangle.h"

namespace silencer::ui::primitives {

void Rectangle(Clay_String id,
               Uint16 width,
               Uint16 height,
               Uint8  fillPaletteColor,
               Uint8  fillOpacity,
               Uint8  strokePaletteColor,
               Uint8  strokeWidth) {
	// Sizing: 0 on either axis means "grow into the parent's available space"
	// (use inside a flex container). Non-zero is a fixed pixel size.
	Clay_SizingAxis xSizing = (width == 0)
		? CLAY_SIZING_GROW(0)
		: CLAY_SIZING_FIXED(static_cast<float>(width));
	Clay_SizingAxis ySizing = (height == 0)
		? CLAY_SIZING_GROW(0)
		: CLAY_SIZING_FIXED(static_cast<float>(height));

	// Fill: Clay only emits a RECTANGLE render command when
	// `.backgroundColor.a > 0`. Set alpha to the requested opacity (which
	// the C1 bridge will eventually route to the palette alpha-blend LUT).
	// `.r` carries the palette index for the bridge.
	Clay_Color bg{
		/*r=*/static_cast<float>(fillPaletteColor),
		/*g=*/0.0f,
		/*b=*/0.0f,
		/*a=*/static_cast<float>(fillOpacity),
	};

	// Stroke: Clay only emits a BORDER render command when at least one
	// edge width is non-zero. strokeWidth==0 disables the border entirely.
	Clay_BorderElementConfig border{
		.color = {
			/*r=*/static_cast<float>(strokePaletteColor),
			/*g=*/0.0f, /*b=*/0.0f, /*a=*/255.0f,
		},
		.width = {
			/*left=*/strokeWidth, /*right=*/strokeWidth,
			/*top=*/strokeWidth, /*bottom=*/strokeWidth,
			/*betweenChildren=*/0,
		},
	};

	CLAY({
		.id = Clay_GetElementId(id),
		.layout = { .sizing = { xSizing, ySizing } },
		.backgroundColor = bg,
		.border = border,
	}) {}
}

}  // namespace silencer::ui::primitives
