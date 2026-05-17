#include "bank_text.h"

#include "clay_ui_payloads.h"

namespace silencer::ui::primitives {

namespace {

struct VariantConfig {
	Uint16 bank;
	Uint16 cellWidth;
};

VariantConfig ConfigFor(BankTextVariant v) {
	switch(v){
		case BankTextVariant::Title:   return {135, 11};
		case BankTextVariant::Heading: return {134, 8};
		case BankTextVariant::Body:    return {133, 6};
		case BankTextVariant::BodySm:  return {133, 7};
	}
	return {133, 6};
}

bool OptsAreDefault(const BankTextOpts & o) {
	return o.brightness == 128 && !o.colorRamp && !o.drawAlpha && !o.measureInk;
}

// Per-frame arena for BankTextDrawData userData payloads. Allocations stay
// pointer-stable for the duration of one Clay layout pass — fixed-size raw
// array, no realloc. Reset by BankTextBeginFrame().
constexpr int kBankTextUserDataCapacity = 1024;
silencer::clay_bridge::BankTextDrawData g_userDataArena[kBankTextUserDataCapacity];
int g_userDataCount = 0;

silencer::clay_bridge::BankTextDrawData * AllocUserData(const BankTextOpts & o) {
	if(g_userDataCount >= kBankTextUserDataCapacity) return nullptr;
	auto * slot = &g_userDataArena[g_userDataCount++];
	slot->brightness = o.brightness;
	slot->colorRamp  = o.colorRamp;
	slot->drawAlpha  = o.drawAlpha;
	slot->measureInk = o.measureInk;
	return slot;
}

}  // namespace

void BankTextBeginFrame() {
	g_userDataCount = 0;
}

void BankText(Clay_String text, BankTextVariant variant, BankTextOpts opts) {
	VariantConfig vc = ConfigFor(variant);
	void * userData = OptsAreDefault(opts)
		? nullptr
		: static_cast<void *>(AllocUserData(opts));
	CLAY_TEXT(text,
	          CLAY_TEXT_CONFIG({
	              .userData  = userData,
	              .textColor = { static_cast<float>(opts.effectColor),
	                             0.0f, 0.0f, 255.0f },
	              .fontId    = vc.bank,
	              .fontSize  = vc.cellWidth,
	          }));
}

}  // namespace silencer::ui::primitives
