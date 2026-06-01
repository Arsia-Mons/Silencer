#include "client/ui/hooks/use_app.h"

#include "audio.h"
#include "game.h"
#include "gasloader.h"
#include "resources.h"
#include "screen_context.h"
#include "world.h"

#include <memory>

namespace silencer {
namespace client_ui {

struct AppProviderState {
	Game * game = nullptr;
	Resources * resources = nullptr;
	World * world = nullptr;
};

namespace app_provider_detail {

constexpr std::uint16_t kAgencyEmblemBank = 181;
constexpr std::uint16_t kReadyIndicatorBank = 7;
constexpr std::uint16_t kReadyIndicatorReadyFrame = 18;
constexpr std::uint16_t kReadyIndicatorWaitingFrame = 19;

Game * GameFor(const AppProviderValue& provider) {
	return provider.state ? provider.state->game : nullptr;
}

Resources * ResourcesFor(const AppProviderValue& provider) {
	return provider.state ? provider.state->resources : nullptr;
}

World * WorldFor(const AppProviderValue& provider) {
	return provider.state ? provider.state->world : nullptr;
}

}  // namespace app_provider_detail

AppProviderValue MakeAppProvider(ScreenContext& ctx) {
	AppProviderValue value;
	value.state = std::make_shared<AppProviderState>();
	value.state->game = &ctx.game;
	value.state->resources = &ctx.world.resources;
	value.state->world = &ctx.world;
	return value;
}

AppLifecycleModel::AppLifecycleModel(const AppProviderValue& provider)
	: provider_(provider) {}

void AppLifecycleModel::quit() const {
	if(Game * game = app_provider_detail::GameFor(provider_)){
		game->quitRequested = true;
	}
}

AppAssetsModel::AppAssetsModel(const AppProviderValue& provider)
	: provider_(provider) {}

namespace {

AppSpriteFrame SpriteFrame(const AppProviderValue& provider,
                           std::uint16_t bank,
                           std::uint16_t frame) {
	AppSpriteFrame out;
	Resources * resources = app_provider_detail::ResourcesFor(provider);
	if(!resources) return out;

	out.offset_x = resources->spriteoffsetx[bank][frame];
	out.offset_y = resources->spriteoffsety[bank][frame];
	out.left = -out.offset_x;
	out.top = -out.offset_y;
	out.width = static_cast<int>(resources->spritewidth[bank][frame]);
	out.height = static_cast<int>(resources->spriteheight[bank][frame]);
	out.available = out.width > 0 && out.height > 0;
	return out;
}

}  // namespace

AppSpriteFrame AppAssetsModel::agency_emblem(Uint8 agency) const {
	return SpriteFrame(provider_, app_provider_detail::kAgencyEmblemBank, agency);
}

AppSpriteFrame AppAssetsModel::ready_indicator(bool ready) const {
	return SpriteFrame(provider_,
	                   app_provider_detail::kReadyIndicatorBank,
	                   ready
	                       ? app_provider_detail::kReadyIndicatorReadyFrame
	                       : app_provider_detail::kReadyIndicatorWaitingFrame);
}

AppAudioModel::AppAudioModel(const AppProviderValue& provider)
	: provider_(provider) {}

void AppAudioModel::play_ui_click() const {
#ifdef SILENCER_TEST_BUILD
	(void)provider_;
#else
	Resources * resources = app_provider_detail::ResourcesFor(provider_);
	if(!resources) return;
	const std::string & sound = GASLoader::Get().player.soundUIClick;
	auto it = resources->soundbank.find(sound);
	if(it != resources->soundbank.end() && it->second){
		Audio::GetInstance().PlayUI(it->second);
	}
#endif
}

AppModel::AppModel(const AppProviderValue& provider)
	: assets(provider), audio(provider), lifecycle(provider), provider_(provider) {}

std::string AppModel::version() const {
	World * world = app_provider_detail::WorldFor(provider_);
	return world ? world->GetVersion() : std::string();
}

AppModel use_app(const AppProviderValue& provider) {
	return AppModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
