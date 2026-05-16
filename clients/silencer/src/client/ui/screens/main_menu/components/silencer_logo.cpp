#include "silencer_logo.h"

#include "clay/clay.h"
#include "resources.h"

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace silencer::client_ui::main_menu {

namespace silencer_logo_detail {

constexpr std::uint16_t kLogoBank = 208;
constexpr std::uint16_t kFirstFrame = 29;
constexpr std::uint16_t kHeldFrame = 60;
constexpr std::uint64_t kLegacyTickMs = 42;
constexpr std::uint64_t kRevealTicks = 60;
constexpr std::uint64_t kHeldEndTicks = 180;
constexpr std::uint64_t kCycleTicks = 240;

}  // namespace silencer_logo_detail

void SilencerLogo::Reset() {
	started_ = true;
	startMs_ = SDL_GetTicks();
}

std::uint16_t SilencerLogo::CurrentFrame(std::uint64_t nowMs) const {
	if(!started_ || nowMs <= startMs_) return silencer_logo_detail::kFirstFrame;

	const std::uint64_t legacyTick =
		((nowMs - startMs_) / silencer_logo_detail::kLegacyTickMs) %
		silencer_logo_detail::kCycleTicks;

	if(legacyTick < silencer_logo_detail::kRevealTicks){
		return static_cast<std::uint16_t>(
			silencer_logo_detail::kFirstFrame + legacyTick / 2);
	}
	if(legacyTick < silencer_logo_detail::kHeldEndTicks){
		return silencer_logo_detail::kHeldFrame;
	}

	const int frame = 120 - static_cast<int>(legacyTick / 2) +
	                  silencer_logo_detail::kFirstFrame;
	return static_cast<std::uint16_t>(
		frame <= silencer_logo_detail::kFirstFrame
			? silencer_logo_detail::kFirstFrame
			: frame);
}

bool SilencerLogo::EnsureBounds(Resources & resources) {
	if(boundsCached_) return boundsWidth_ > 0 && boundsHeight_ > 0;

	const std::uint16_t bank = silencer_logo_detail::kLogoBank;
	int minLeft = std::numeric_limits<int>::max();
	int minTop = std::numeric_limits<int>::max();
	int maxRight = std::numeric_limits<int>::min();
	int maxBottom = std::numeric_limits<int>::min();

	for(std::uint16_t frame = silencer_logo_detail::kFirstFrame;
	    frame <= silencer_logo_detail::kHeldFrame;
	    ++frame){
		const int left = -resources.spriteoffsetx[bank][frame];
		const int top = -resources.spriteoffsety[bank][frame];
		const int right = left + static_cast<int>(resources.spritewidth[bank][frame]);
		const int bottom = top + static_cast<int>(resources.spriteheight[bank][frame]);
		minLeft = std::min(minLeft, left);
		minTop = std::min(minTop, top);
		maxRight = std::max(maxRight, right);
		maxBottom = std::max(maxBottom, bottom);
	}

	boundsMinLeft_ = minLeft;
	boundsMinTop_ = minTop;
	boundsWidth_ = maxRight - minLeft;
	boundsHeight_ = maxBottom - minTop;
	boundsCached_ = true;
	return boundsWidth_ > 0 && boundsHeight_ > 0;
}

void SilencerLogo::Build(Resources & resources) {
	if(!started_) Reset();
	if(!EnsureBounds(resources)) return;

	using namespace silencer::clay_bridge;

	const std::uint16_t frame = CurrentFrame(SDL_GetTicks());
	const int frameLegacyLeft =
		-resources.spriteoffsetx[silencer_logo_detail::kLogoBank][frame];
	const int frameLegacyTop =
		-resources.spriteoffsety[silencer_logo_detail::kLogoBank][frame];
	spritePayload_ = SpritePayload{
		static_cast<Uint8>(silencer_logo_detail::kLogoBank),
		frame,
		0,
		0,
		0,
		0,
		0,
		128,
		0,
		0,
		static_cast<Sint16>(frameLegacyLeft - boundsMinLeft_),
		static_cast<Sint16>(frameLegacyTop - boundsMinTop_),
	};
	customData_ = ClayCustomData{CustomKind::Sprite, &spritePayload_};

	CLAY({ .id = CLAY_ID("MainMenuSilencerLogoStage"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(boundsWidth_)),
	                       CLAY_SIZING_FIXED(static_cast<float>(boundsHeight_)) },
	       },
	       .custom = { .customData = &customData_ } }) {}
}

}  // namespace silencer::client_ui::main_menu
