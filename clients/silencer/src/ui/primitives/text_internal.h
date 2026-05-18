#ifndef SILENCER_UI_PRIMITIVES_TEXT_INTERNAL_H
#define SILENCER_UI_PRIMITIVES_TEXT_INTERNAL_H

#include "primitives/text.h"

namespace silencer::ui::primitives::text_internal {

struct TextRenderStyle {
	Uint16 bank;
	Uint16 advance;
	Uint16 lineHeight;
};

struct InternalTextOpts {
	bool measureInk = false;
};

TextRenderStyle ResolveTextRenderStyle(TextSize size);

void TextWithInternalOptions(Clay_String text,
                             TextOpts opts,
                             InternalTextOpts internalOpts = InternalTextOpts{});

}  // namespace silencer::ui::primitives::text_internal

#endif
