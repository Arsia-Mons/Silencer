#ifndef GAME_H
#define GAME_H

#include "renderdevice.h"
#include "renderer.h"
#include "input.h"
#include "state.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"
#include "textbox.h"
#include "updater.h"
#include "controlserver.h"
#include <map>
#include <atomic>
#include <thread>

class Game
{
public:
	Game();
	~Game();
	bool Load(char * cmdline);
	bool Loop(void);
	bool HandleSDLEvents(void);
	void LoadProgressCallback(int progress, int totalprogressitems);

	friend class Audio;

public:
	// Exposed for ControlDispatch (game-thread only).
	int GetFrameCount() const { return frames; }
	static const char* StateName(Uint8 s);
	Uint8 GetState() const { return state; }
	Uint16 GetCurrentInterfaceId() const { return currentinterface; }
	class World& GetWorld() { return world; }
	nlohmann::json GetWorldSummary();
	const Surface& GetScreenBuffer() const { return screenbuffer; }
	const SDL_Color* GetPaletteColors() const { return palettecolors; }
	Renderer& GetRenderer() { return renderer; }
	bool IsLiveMultiplayer() const;
	bool GoBack(void);
	bool quitRequested = false;
	bool paused;
	int stepFramesRemaining;
	Uint64 stepWallclockDeadlineMs;
	int controlPort;
	bool headless;

private:
	bool Tick(void);
	void Present(void);
	bool SetupRenderDevice(void);
	static Uint32 TimerCallback(void * userdata, SDL_TimerID timerID, Uint32 interval);
	void SetColors(SDL_Color * colors);
	void UpdateInputState(Input & input);
	bool LoadMap(const char * name);
	void UnloadGame(void);
	bool CheckForQuit(void);
	bool CheckForEndOfGame(void);
	bool CheckForConnectionLost(void);
	void ProcessInGameInterfaces(void);
	void ShowDeployMessage(void);
	void GiveDefaultItems(Player & player);
	void JoinGame(LobbyGame & lobbygame, char * password = 0);
	void GoToState(Uint8 newstate);
	Interface * CreateMainMenuInterface(void);
	Interface * CreateOptionsInterface(void);
	Interface * CreateOptionsControlsInterface(void);
	Interface * CreateOptionsDisplayInterface(void);
	Interface * CreateOptionsAudioInterface(void);
	Interface * CreateLobbyConnectInterface(void);
	Interface * CreateLobbyInterface(void);
	Interface * CreateCharacterInterface(void);
	Interface * CreateGameSelectInterface(void);
	Interface * CreateChatInterface(void);
	Interface * CreateGameCreateInterface(void);
	Interface * CreateGameJoinInterface(void);
	Interface * CreateGameTechInterface(void);
	Interface * CreateGameSummaryInterface(Stats & stats, Uint8 agency);
	Interface * CreateModalDialog(const char * message, bool ok = true);
	Interface * CreateUpdateInterface(void);
	void ProcessUpdateInterface(Interface * iface);
	void LaunchStage2(void);
	Interface * CreateMapPreview(const char * filename);
	void PlayMusic(Mix_Music * music);
	void DestroyModalDialog(void);
	Interface * CreatePasswordDialog(void);
	Uint16 lobbyinterface;
	Uint16 characterinterface;
	Uint16 chatinterface;
	Uint16 gameselectinterface;
	Uint16 gamecreateinterface;
	Uint16 gamejoininterface;
	Uint16 gametechinterface;
	Uint16 gamesummaryinterface;
	Uint16 modalinterface;
	Uint16 passwordinterface;
	Uint16 mappreviewinterface;
	Uint16 updateinterface;
	Updater updater;
	Overlay * keynameoverlay[6];
	Button * c1button[6];
	Button * cobutton[6];
	Button * c2button[6];
	bool ProcessMainMenuInterface(Interface * iface);
	void ProcessLobbyConnectInterface(Interface * iface);
	bool ProcessLobbyInterface(Interface * iface);
	void ProcessGameSummaryInterface(Interface * iface);
	void UpdateLobbyMapName(const char * name);
	void UpdateTechInterface(void);
	void UpdateGameSummaryInterface(void);
	void AddSummaryLine(TextBox & textbox, const char * name, Uint32 value, bool percentage = false);
	void ShowTeamOverlays(bool show);
	Uint8 GetSelectedAgency(void);
	void IndexToConfigKey(int index, SDL_Scancode ** key1, SDL_Scancode ** key2, bool ** keyop);
	const char * GetKeyName(SDL_Scancode sym);
	void GetGameChannelName(LobbyGame & lobbygame, char * name);
	void CreateAmbienceChannels(void);
	void UpdateAmbienceChannels(void);
	bool FadedIn(void);
	std::vector<std::string> ListFiles(const char * directory);
	void LoadRandomGameMusic(void);
	std::string FindMap(const char * name, unsigned char (*hash)[20] = 0, const char * directory = 0);
	std::string SaveMap(const char * name, unsigned char * data, int size);
	bool CalculateMapHash(const char * filename, unsigned char (*hash)[20]);
	std::string StringFromHash(unsigned char (*hash)[20]);
	void LoadMapData(const char * filename);
	void ProcessMapDownload(void);
	static const int numkeys = 20;
	const char * keynames[numkeys];
	Uint8 keystate[SDL_SCANCODE_COUNT];
	enum {NONE, FADEOUT, MAINMENU, LOBBYCONNECT, LOBBY, UPDATING, INGAME, MISSIONSUMMARY, SINGLEPLAYERGAME, OPTIONS, OPTIONSCONTROLS, OPTIONSDISPLAY, OPTIONSAUDIO, HOSTGAME, JOINGAME, REPLAYGAME, TESTGAME};
	Uint8 state;
	Uint8 nextstate;
	Uint8 fade_i;
	bool stateisnew;
	bool nextstateprocessed;
	class World world;
	Renderer renderer;
	SDL_Window * window;
	RenderDevice * renderdevice;
	SDL_Color palettecolors[256]; // CPU copy — for ffmpeg replay pixel export
	Surface screenbuffer;
	int frames;
	int fps;
	Uint64 lasttick;
	Uint16 currentinterface;
	Uint16 aftermodalinterface;
	bool motdprinted;
	Uint32 chatlinesprinted;
	char localusername[16 + 1];
	Uint16 sharedstate;
	int bgchannel[3];
	enum {BG_AMBIENT = 0, BG_BASE, BG_OUTSIDE};
	int oldselecteditem;
	Uint8 singleplayermessage;
	bool updatetitle;
	Uint32 currentlobbygameid;
	Uint32 lastannouncedgameid;
	Uint8 lastannouncedstatus;
	char lastchannel[64];
	Uint8 oldselectedagency;
	Uint8 oldambiencelevel;
	bool agencychanged;
	bool gamesummaryinfoloaded;
	bool minimized;
	bool creategameclicked;
	bool modaldialoghasok;
	bool joininggame;
	bool deploymessageshown;
	Uint32 optionscontrolstick;
	int quitscancode;
	bool interfaceenterfix;
	Uint32 lastmapchunkrequest;
	bool mapexistchecked;
	int selectedmap;
	std::map<std::string, std::string> servermaps; // "[↓] NAME.SIL" → sha1hex
	std::atomic<int> dlprogress{0};    // 0-100 while downloading
	std::atomic<int> dlresult{0};      // 0=idle, 1=success, -1=fail
	std::string dlitemname;            // key in servermaps being downloaded
	std::thread dlthread;
	Uint32 lastmusicplaytime;
	char currentmusictrack[256];
	bool fullscreentoggled;
	char * replayfile;
	// Set when UpdaterStage2 has been spawned; next Loop() returns false so
	// main unwinds and ~Game tears down SDL/audio cleanly before the new
	// client process opens the device. Skipping this teardown produces an
	// audible pop on the restarted client.
	bool stage2spawned;
	ControlServer controlserver;
	void DrainControlQueue();
	void PostFrameReplies();
};

#endif