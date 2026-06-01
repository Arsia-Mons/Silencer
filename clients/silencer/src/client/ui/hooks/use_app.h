#pragma once

#include "client/ui/providers/app_provider.h"
#include "shared.h"

#include <cstdint>
#include <string>

namespace silencer {
namespace client_ui {

class AppLifecycleModel {
public:
	explicit AppLifecycleModel(const AppProviderValue& provider);

	void quit() const;

private:
	AppProviderValue provider_;
};

struct AppSpriteFrame {
	bool available = false;
	int left = 0;
	int top = 0;
	int offset_x = 0;
	int offset_y = 0;
	int width = 0;
	int height = 0;
};

class AppAssetsModel {
public:
	explicit AppAssetsModel(const AppProviderValue& provider);

	AppSpriteFrame agency_emblem(Uint8 agency) const;
	AppSpriteFrame ready_indicator(bool ready) const;

private:
	AppProviderValue provider_;
};

class AppAudioModel {
public:
	explicit AppAudioModel(const AppProviderValue& provider);

	void play_ui_click() const;

private:
	AppProviderValue provider_;
};

class AppModel {
public:
	explicit AppModel(const AppProviderValue& provider);

	std::string version() const;

	AppAssetsModel assets;
	AppAudioModel audio;
	AppLifecycleModel lifecycle;

private:
	AppProviderValue provider_;
};

AppModel use_app(const AppProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
