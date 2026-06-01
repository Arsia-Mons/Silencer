#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include "ambience_mixer.h"
#include "map_downloader.h"

class Game;
class LobbyGame;
class Player;

class GameSession
{
public:
explicit GameSession(Game & game);

bool LoadMap(const char * name);
void UnloadGame();
void JoinGame(LobbyGame & lobbygame, char * password);
void SpectateGame(LobbyGame & lobbygame, char * password);
void LeaveJoinedGame();
bool CheckForQuit();
bool CheckForEndOfGame();
bool CheckForConnectionLost();
void ShowDeployMessage();
void GiveDefaultItems(Player & player);

MapDownloader & MapDownloaderRef() { return mapDownloader; }
AmbienceMixer & AmbienceMixerRef() { return ambienceMixer; }
Uint32 CurrentLobbyGameId() const { return currentlobbygameid; }
void SetCurrentLobbyGameId(Uint32 gameid) { currentlobbygameid = gameid; }
bool IsJoiningGame() const { return joininggame; }
void ClearJoiningGame() { joininggame = false; }
Uint32 & LastAnnouncedGameIdRef() { return lastannouncedgameid; }
Uint8 & LastAnnouncedStatusRef() { return lastannouncedstatus; }
bool & DeployMessageShownRef() { return deploymessageshown; }

private:
Game & game;
MapDownloader mapDownloader;
AmbienceMixer ambienceMixer;
Uint32 currentlobbygameid;
Uint32 lastannouncedgameid;
Uint8 lastannouncedstatus;
bool deploymessageshown;
bool joininggame;
};

#endif
