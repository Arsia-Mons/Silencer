#include "label_value_row.h"

namespace silencer::ui::primitives {

void LabelValueRow(Clay_String id,
                   Clay_String label,
                   Clay_String value,
                   LabelValueRowOpts opts) {
	const float rowW   = static_cast<float>(opts.width);
	const float rowH   = static_cast<float>(opts.height);
	const float labelW = static_cast<float>(opts.labelWidth);
	float valueW = rowW - labelW;
	if(valueW < 0.0f) valueW = 0.0f;

	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(rowW),
	                       CLAY_SIZING_FIXED(rowH) },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		// Label column.
		CLAY({ .id = CLAY_SIDI(id, 1),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(labelW),
		                       CLAY_SIZING_FIXED(rowH) },
		       } }) {
			TextOpts labelText = opts.text;
			if(opts.labelEffect.HasOverride()) labelText.effect = opts.labelEffect;
			Text(label, labelText);
		}
		// Value column.
		CLAY({ .id = CLAY_SIDI(id, 2),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(valueW),
		                       CLAY_SIZING_FIXED(rowH) },
		       } }) {
			TextOpts valueText = opts.text;
			if(opts.valueEffect.HasOverride()) valueText.effect = opts.valueEffect;
			Text(value, valueText);
		}
	}
}

}  // namespace silencer::ui::primitives
