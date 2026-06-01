#include "lobby_connect_screen.h"

#include "client/ui/screens/character_create/character_create_screen.h"
#include "client/ui/screens/lobby_connect/lobby_connect_view.h"
#include "client/ui/screens/lobby/lobby_screen.h"
#include "client/ui/screens/main_menu/main_menu_screen.h"
#include "client/ui/screens/update/update_screen.h"
#include "screen_context.h"
#include "lobby.h"
#include "updater.h"
#include "ambience_mixer.h"
#include "config.h"
#include "peer.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>

namespace lobby_connect_screen_detail
{
constexpr const char * kActionUsername = "lobby_connect.username";
constexpr const char * kActionPassword = "lobby_connect.password";

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

void SubmitCredentials(World & world, const char * username, const char * password)
{
	world.lobby.LockMutex();
	if(world.lobby.state == Lobby::AUTHENTICATING){
		world.lobby.SetLocalUsername(username);
		world.lobby.SendCredentials(username, password);
		world.lobby.state = Lobby::AUTHSENT;
	}
	world.lobby.UnlockMutex();
}
} // namespace lobby_connect_screen_detail

void LobbyConnectScreen::Build(ScreenContext & ctx)
{
	ctx.world.GetAuthorityPeer()->controlledlist.clear();
	ctx.world.DestroyAllObjects();
	ctx.world.lobby.ClearGames();
	ctx.world.lobby.state = Lobby::WAITING;
	ctx.ResetPresentation(2);
	motdprinted = false;
	logLines.clear();
	username[0] = '\0';
	password[0] = '\0';
}

void LobbyConnectScreen::Tick(ScreenContext & ctx)
{
	ctx.PlayMenuMusicIfReady();
	// Mirror the legacy lobby-connect gate: nothing happens until
	// the menu music has crossfaded in. The lobby state machine starts in
	// WAITING and only kicks off the TCP connect on the first tick after
	// the gate opens, so this delay shapes when the user sees "Connecting
	// to ..." appear in the textbox.
	if(!ctx.ambienceMixer.FadedIn()) return;

	World & world = ctx.world;
	world.lobby.LockMutex();
	switch(world.lobby.state){
		case Lobby::CONNECTING:

		break;
		case Lobby::WAITINGFORRESOLVER:

		break;
		case Lobby::AUTHSENT:

		break;
		case Lobby::IDLE:

		break;
		case Lobby::WAITING:{
			char line[128];
			snprintf(line, sizeof(line), "Connecting to %s:%d",
			         Config::GetInstance().lobbyhost,
			         Config::GetInstance().lobbyport);
			AppendLog(line);
			world.lobby.Connect(Config::GetInstance().lobbyhost,
			                    Config::GetInstance().lobbyport);
			//world.lobby.state = Lobby::AUTHENTICATED;
		}break;
		case Lobby::RESOLVING:
			AppendLog("Resolving hostname...");
			world.lobby.state = Lobby::WAITINGFORRESOLVER;
		break;
		case Lobby::RESOLVEFAILED:
			AppendLog("Could not resolve hostname");
			//world.lobby.Disconnect();
			world.lobby.state = Lobby::IDLE;
		break;
		case Lobby::RESOLVED:
			AppendLog("Hostname resolved");
			world.lobby.Connect(Config::GetInstance().lobbyhost,
			                    Config::GetInstance().lobbyport);
		break;
		case Lobby::CONNECTED:
			AppendLog("Connected");
			AppendLog("Checking version...");
			world.lobby.SendVersion();
			world.lobby.state = Lobby::CHECKINGVERSION;
		break;
		case Lobby::CHECKINGVERSION:
			if(world.lobby.versionchecked){
				if(world.lobby.versionok){
					AppendLog("Software version is current");
					world.lobby.state = Lobby::AUTHENTICATING;
				}else{
					if(world.lobby.updateavailable){
						// Route into the auto-updater flow.
						ctx.updater.PresentUpdate(world.lobby.updateurl,
						                          world.lobby.updatesha256);
						world.lobby.Disconnect();
						world.lobby.state = Lobby::IDLE;
						world.lobby.UnlockMutex();
						ctx.ResetToScreen(std::make_unique<UpdateScreen>());
						return;
					}else{
						AppendLog("Software is out of date");
						AppendLog("Get latest version at:");
						AppendLog("https://github.com/Arsia-Mons/Silencer");
						world.lobby.Disconnect();
						world.lobby.state = Lobby::IDLE;
					}
				}
			}
		break;
		case Lobby::AUTHENTICATING:
			//world.lobby.state = Lobby::AUTHENTICATED;
		break;
		case Lobby::AUTHFAILED:
			AppendLog("Authentication failed");
			if(strlen(world.lobby.failmessage) > 0){
				AppendLog(world.lobby.failmessage);
			}
			world.lobby.state = Lobby::AUTHENTICATING;
			//world.lobby.Disconnect();
		break;
		case Lobby::AUTHENTICATED:
			if(!world.lobby.charactersreceived){
				break;
			}
			AppendLog("Authenticated");
			{
				const bool needsCharacter = world.lobby.characters.empty();
				world.lobby.UnlockMutex();
				if(needsCharacter){
					ctx.ResetToScreen(std::make_unique<CharacterCreateScreen>());
				}else{
					ctx.ResetToScreen(std::make_unique<LobbyScreen>());
				}
				return;
			}
		case Lobby::CONNECTIONFAILED:
			AppendLog("Connection failed");
			world.lobby.state = Lobby::IDLE;
		break;
		case Lobby::DISCONNECTED:
			AppendLog("Disconnected");
			world.lobby.state = Lobby::IDLE;
		break;
	}
	if(world.lobby.motdreceived && !motdprinted){
		char * line = strtok(world.lobby.motd, "\n");
		while(line != 0){
			AppendLog(line);
			line = strtok(NULL, "\n");
		}
		motdprinted = true;
	}
	world.lobby.UnlockMutex();
}

bool LobbyConnectScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	std::array<const char *, silencer::client_ui::kLobbyConnectLogLineCount> lines = {};
	const int lineCount = static_cast<int>(logLines.size());
	int start = lineCount - silencer::client_ui::kLobbyConnectLogLineCount;
	if(start < 0) start = 0;
	for(int i = 0; i < silencer::client_ui::kLobbyConnectLogLineCount; i++){
		const int source = start + i;
		lines[i] = source >= 0 && source < lineCount ? logLines[source].c_str() : "";
	}

	const bool inactive = ctx.world.lobby.state == Lobby::AUTHSENT;
	const silencer::client_ui::LobbyConnectLog log{
		.log_lines = lines,
	};
	const silencer::client_ui::LobbyConnectCredentials credentials{
		.username = username,
		.password = password,
		.inactive = inactive,
		.set_username = [this](const std::string& value) {
			lobby_connect_screen_detail::CopyUiText(
				username, static_cast<int>(sizeof(username)), value);
		},
		.set_password = [this](const std::string& value) {
			lobby_connect_screen_detail::CopyUiText(
				password, static_cast<int>(sizeof(password)), value);
		},
		.submit = [this, world = &ctx.world]() {
			lobby_connect_screen_detail::SubmitCredentials(*world, username, password);
		},
	};
	const auto * storedLog = ::ui::copy_value(log);
	const auto * storedCredentials = ::ui::copy_value(credentials);
	if(!storedLog || !storedCredentials) return false;
	*out = ::ui::component(
		"LobbyConnectView",
		silencer::client_ui::LobbyConnectViewProps{
			.key = "lobby-connect",
			.log = storedLog,
			.credentials = storedCredentials,
		},
		silencer::client_ui::LobbyConnectView);
	return true;
}

void LobbyConnectScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool LobbyConnectScreen::HandleBack(ScreenContext & ctx)
{
	ctx.ResetToScreen(std::make_unique<MainMenuScreen>());
	return true;
}

bool LobbyConnectScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::SetText){
		if(action.id == lobby_connect_screen_detail::kActionUsername){
			lobby_connect_screen_detail::CopyUiText(username, static_cast<int>(sizeof(username)), action.value);
			return true;
		}
		if(action.id == lobby_connect_screen_detail::kActionPassword){
			lobby_connect_screen_detail::CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText &&
	   (action.id == lobby_connect_screen_detail::kActionUsername || action.id == lobby_connect_screen_detail::kActionPassword)){
		if(action.id == lobby_connect_screen_detail::kActionUsername){
			lobby_connect_screen_detail::CopyUiText(username, static_cast<int>(sizeof(username)), action.value);
		}else{
			lobby_connect_screen_detail::CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
		}
		lobby_connect_screen_detail::SubmitCredentials(ctx.world, username, password);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Cancel) return HandleBack(ctx);
	return false;
}

void LobbyConnectScreen::AppendLog(const char * text)
{
	if(!text) return;
	logLines.push_back(text);
	if(logLines.size() > 256){
		logLines.erase(logLines.begin(),
		               logLines.begin() + (logLines.size() - 256));
	}
}
