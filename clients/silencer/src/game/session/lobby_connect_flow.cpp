#include "session/lobby_connect_flow.h"

#include "game.h"
#include "lobby.h"
#include "updater.h"
#include "world.h"
#include "config.h"
#include "game/state/game_state.h"

#include <cstdio>
#include <cstring>

void LobbyConnectFlow::Reset(){
	log_.clear();
	motdPrinted_ = false;
}

void LobbyConnectFlow::AddLine(const char * text){
	if(!text) return;
	log_.push_back(text);
	// Match the legacy 256-line scrollback cap.
	if(log_.size() > 256){
		log_.erase(log_.begin(), log_.begin() + (log_.size() - 256));
	}
}

void LobbyConnectFlow::AddMotdLines(const char * motd){
	if(!motd) return;
	std::string text = motd;
	size_t start = 0;
	while(start < text.size()){
		size_t end = text.find('\n', start);
		std::string line = text.substr(start, end == std::string::npos ? end : end - start);
		if(!line.empty()) AddLine(line.c_str());
		if(end == std::string::npos) break;
		start = end + 1;
	}
}

void LobbyConnectFlow::Advance(Game & game, Updater & updater){
	using namespace GameState;
	Lobby & lobby = game.GetWorld().lobby;

	// Captured under the lock, acted on after it: routing flips the game state
	// and presenting an update touches the (separately-locked) updater, neither
	// of which should run while we hold the lobby mutex.
	int routeState = -1;
	bool presentUpdate = false;
	std::string updateUrl;
	uint8_t updateSha[32] = {0};

	lobby.LockMutex();
	switch(lobby.state){
		case Lobby::WAITING:{
			char line[128];
			snprintf(line, sizeof(line), "Connecting to %s:%d",
			         Config::GetInstance().lobbyhost,
			         Config::GetInstance().lobbyport);
			AddLine(line);
			lobby.Connect(Config::GetInstance().lobbyhost,
			              Config::GetInstance().lobbyport);
		}break;
		case Lobby::RESOLVING:
			AddLine("Resolving hostname...");
			lobby.state = Lobby::WAITINGFORRESOLVER;
		break;
		case Lobby::RESOLVEFAILED:
			AddLine("Could not resolve hostname");
			lobby.state = Lobby::IDLE;
		break;
		case Lobby::RESOLVED:
			AddLine("Hostname resolved");
			lobby.Connect(Config::GetInstance().lobbyhost,
			              Config::GetInstance().lobbyport);
		break;
		case Lobby::CONNECTED:
			AddLine("Connected");
			AddLine("Checking version...");
			lobby.SendVersion();
			lobby.state = Lobby::CHECKINGVERSION;
		break;
		case Lobby::CHECKINGVERSION:
			if(lobby.versionchecked){
				if(lobby.versionok){
					AddLine("Software version is current");
					lobby.state = Lobby::AUTHENTICATING;
				}else if(lobby.updateavailable){
					updateUrl = lobby.updateurl;
					memcpy(updateSha, lobby.updatesha256, sizeof(updateSha));
					presentUpdate = true;
					lobby.Disconnect();
					lobby.state = Lobby::IDLE;
					routeState = UPDATING;
				}else{
					AddLine("Software is out of date");
					AddLine("Get latest version at:");
					AddLine("https://github.com/Arsia-Mons/Silencer");
					lobby.Disconnect();
					lobby.state = Lobby::IDLE;
				}
			}
		break;
		case Lobby::AUTHFAILED:
			AddLine("Authentication failed");
			if(strlen(lobby.failmessage) > 0) AddLine(lobby.failmessage);
			lobby.state = Lobby::AUTHENTICATING;
		break;
		case Lobby::AUTHENTICATED:
			// Hold here until the character roster arrives, then route: a fresh
			// account (no characters) goes to creation, otherwise the lobby.
			if(lobby.charactersreceived){
				AddLine("Authenticated");
				routeState = lobby.characters.empty() ? CREATECHARACTER : LOBBY;
			}
		break;
		case Lobby::CONNECTIONFAILED:
			AddLine("Connection failed");
			lobby.state = Lobby::IDLE;
		break;
		case Lobby::DISCONNECTED:
			AddLine("Disconnected");
			lobby.state = Lobby::IDLE;
		break;
		// CONNECTING / WAITINGFORRESOLVER / AUTHENTICATING / AUTHSENT / IDLE:
		// nothing to drive — DoNetwork or the user (credential submit) advances.
		default:
		break;
	}
	if(lobby.motdreceived && !motdPrinted_){
		AddMotdLines(lobby.motd);
		motdPrinted_ = true;
	}
	lobby.UnlockMutex();

	if(presentUpdate) updater.PresentUpdate(updateUrl, updateSha);
	if(routeState >= 0) game.GoToState((Uint8)routeState);
}
