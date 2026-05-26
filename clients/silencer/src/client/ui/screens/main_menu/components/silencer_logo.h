#ifndef SILENCER_CLIENT_UI_MAIN_MENU_SILENCER_LOGO_H
#define SILENCER_CLIENT_UI_MAIN_MENU_SILENCER_LOGO_H

#include "render/clay_ui_payloads.h"

#include <cstdint>

class ScreenContext;

namespace silencer::client_ui::main_menu {

class SilencerLogo {
public:
	void Reset();
	void Build(ScreenContext & ctx);

private:
	std::uint64_t startMs_ = 0;
	bool started_ = false;
	bool boundsCached_ = false;
	int boundsMinLeft_ = 0;
	int boundsMinTop_ = 0;
	int boundsWidth_ = 0;
	int boundsHeight_ = 0;
	silencer::clay_bridge::SpritePayload spritePayload_;
	silencer::clay_bridge::ClayCustomData customData_;

	std::uint16_t CurrentFrame(std::uint64_t nowMs) const;
	bool EnsureBounds(ScreenContext & ctx);
};

}  // namespace silencer::client_ui::main_menu

#endif
