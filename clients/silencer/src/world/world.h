#ifndef WORLD_H
#define WORLD_H

#include "shared.h"
#include <vector>
#include "input.h"
#include "object.h"
#include "objecttypes.h"
#include "map.h"
#include "resources.h"
#include "audio.h"
#include "lobby.h"
#include "lobbygame.h"
#include "dedicatedserver.h"
#include "buyableitem.h"
#include "replay.h"
#include "TriggerGraph.h"
#include "gamemode.h"
#include "messaging/world_messaging.h"
#include "objects/world_object_registry.h"
#include "network/world_network.h"
#include "network/world_peer_registry.h"
#include "network/world_replication.h"

class Object;
class Peer;
class Player;
class Team;
class Renderer;
class Surface;
class Serializer;
class Audio;
class BuyableItem;

class World {
    public:
    enum modes { AUTHORITY, REPLICA };
    enum {
        BUY_NONE, BUY_LASER, BUY_ROCKET, BUY_FLAMER, BUY_HEALTH, BUY_TRACT,
        BUY_SECURITYPASS, BUY_VIRUS, BUY_POISON, BUY_EMPB, BUY_SHAPEDB,
        BUY_PLASMAB, BUY_NEUTRONB, BUY_DET, BUY_FIXEDC, BUY_FLARE,
        BUY_POISONFLARE, BUY_CAMERA, BUY_DOOR, BUY_DEFENSE, BUY_INFO,
        BUY_GIVE0, BUY_GIVE1, BUY_GIVE2, BUY_GIVE3
    };

    WorldMessaging    messaging;
    WorldObjectRegistry objects;
    WorldNetwork      network;
    WorldPeerRegistry peers;
    WorldReplication  replication;

    static const int maxstatusmessages = WorldMessaging::maxstatusmessages;

    World(bool mode = AUTHORITY);
    ~World();

    // Camera
    bool    pancameraactive, pancamerareturn;
    Uint32  pancamerareturncount;
    Sint16  pancamerax, pancameray;
    Uint8   viewedpeerid;
    struct SpectatorView {
        bool freecam;
        int  camx, camy, camvx, camvy;
        bool holdshowallnames;
        bool initialized;
    } spectator;

    // Object lifecycle
    Object * CreateObject(Uint8 type, Uint16 id = 0)    { return objects.CreateObject(type, id); }
    Object * GetObjectFromId(Uint16 id)                  { return objects.GetObjectFromId(id); }
    void     MarkDestroyObject(Uint16 id)                { objects.MarkDestroyObject(id); }
    void     DestroyMarkedObjects()                      { objects.DestroyMarkedObjects(); }
    void     DestroyObject(Uint16 id)                    { objects.DestroyObject(id); }
    void     DestroyAllObjects()                         { objects.DestroyAllObjects(); }

    // Core tick
    void         DoNetwork()                             { network.DoNetwork(); }
    void         Tick();
    void         TickObjects();
    void         SetVersion(const char * version);
    const char * GetVersion() const                      { return version; }

    // Network
    bool           Listen(unsigned short port = 0)       { return network.Listen(port); }
    unsigned short Bind(unsigned short port = 0)         { return network.Bind(port); }
    void           Connect(Uint8 agency, Uint32 accountid, Uint32 selectedcharid = 0, const char * password = 0, bool observer = false) { network.Connect(agency, accountid, selectedcharid, password, observer); }
    void           Disconnect()                          { network.Disconnect(); }
    bool           IsAuthority()                         { return network.IsAuthority(); }
    bool           IsConnected() const                   { return network.IsConnected(); }
    bool           IsIdle() const                        { return network.IsIdle(); }
    bool           IsLocalObserver()                     { return network.IsLocalObserver(); }
    void           SwitchToLocalAuthorityMode()          { network.SwitchToLocalAuthorityMode(); }
    void           SendPing()                            { network.SendPing(); }
    int            GetPingTime()                         { return network.GetPingTime(); }
    int            AveragePingJitter()                   { return network.AveragePingJitter(); }
    void           ClearGameplayState()                  { gameplaystate = NONE; }
    void           SetLobbyGameplayState()               { gameplaystate = INLOBBY; }

    // Peers
    Peer *   GetAuthorityPeer()                          { return peers.GetAuthorityPeer(); }
    Peer *   GetPeer(Uint8 peerid)                       { return peers.GetPeer(peerid); }
    Uint8    GetLocalPeerId() const                      { return peers.localpeerid; }
    Player * GetPeerPlayer(Uint8 peerid)                 { return peers.GetPeerPlayer(peerid); }
    Team *   GetPeerTeam(Uint8 peerid)                   { return peers.GetPeerTeam(peerid); }
    bool     FindTeamForPeer(Peer & peer, Uint8 agency, int start = 0) { return peers.FindTeamForPeer(peer, agency, start); }

    // Replication
    void SendInput()                                     { replication.SendInput(); }

    // UI / HUD state
    bool IsShowingPlayerList() const                     { return showplayerlist; }
    void SetShowingPlayerList(bool show)                 { showplayerlist = show; }
    bool IsShowingTeamColors() const                     { return showteamcolors; }
    void SetShowingTeamColors(bool show)                 { showteamcolors = show; }
    bool ShouldHighlightSecrets() const                  { return highlightsecrets; }
    bool ShouldHighlightMinimap() const                  { return highlightminimap; }

    // Messaging
    const char * GetMessageText() const                  { return messaging.GetMessageText(); }
    Uint8        GetMessageProgress() const              { return messaging.GetMessageProgress(); }
    Uint8        GetMessageType() const                  { return messaging.GetMessageType(); }
    Uint8        GetMessageTime() const                  { return messaging.GetMessageTime(); }
    const char * GetTopMessageText() const               { return messaging.GetTopMessageText(); }
    Uint8        GetTopMessageProgress() const           { return messaging.GetTopMessageProgress(); }
    void         ShowMessage(const char * message, Uint8 time = 255, Uint8 type = 0, bool networked = false, Peer * peer = 0) { messaging.ShowMessage(message, time, type, networked, peer); }
    void         ShowStatus(const char * status, Uint8 color = 0, bool networked = false, Peer * peer = 0) { messaging.ShowStatus(status, color, networked, peer); }
    void         ShowTopMessage(const char * message)    { messaging.ShowTopMessage(message); }
    void         SendChat(bool toteam, char * message)   { messaging.SendChat(toteam, message); }
    void         SendSound(const char * name, Peer * peer = 0, Uint8 volume = 128) { messaging.SendSound(name, peer, volume); }
    void         BroadcastTriggerState()                 { messaging.BroadcastTriggerState(); }

    // System camera
    bool   IsSystemCameraActive(int slot) const          { return systemcameraactive[slot]; }
    Uint16 GetSystemCameraFollowId(int slot) const       { return systemcamerafollow[slot]; }
    Sint16 GetSystemCameraX(int slot) const              { return systemcamerax[slot]; }
    Sint16 GetSystemCameraY(int slot) const              { return systemcameray[slot]; }
    void   SetSystemCamera(bool system, Uint16 objectfollow, Sint16 x, Sint16 y);
    void   BroadcastCamera(Sint16 x, Sint16 y);

    // Collision
    const std::vector<Uint16> & GetObjectsByType(Uint8 type) const { return objects.GetObjectsByType(type); }
    bool              TestAABB(int x1, int y1, int x2, int y2, Object * object, std::vector<Uint8> & types, bool onlycollidable = true) { return objects.TestAABB(x1, y1, x2, y2, object, types, onlycollidable); }
    std::vector<Object *> TestAABB(int x1, int y1, int x2, int y2, std::vector<Uint8> & types, Uint16 except = 0, Uint16 teamid = 0, bool onlycollidable = true) { return objects.TestAABB(x1, y1, x2, y2, types, except, teamid, onlycollidable); }
    Object *          TestIncr(int x1, int y1, int x2, int y2, int * xv, int * yv, std::vector<Uint8> & types, Uint16 except = 0, Uint16 teamid = 0) { return objects.TestIncr(x1, y1, x2, y2, xv, yv, types, except, teamid); }

    // Gameplay
    Uint16 GetWinningTeamId() const                      { return winningteamid; }
    void   Illuminate();
    void   ChangeTeam();
    void   SetAgency(Uint8 agency);
    void   KillByGovt(Peer & peer);
    void   Explode(Object & object, Uint8 suitcolor, float hitx);
    bool   IsSecurity(Object & object);
    void   SetRandomSeed(Uint32 seed);
    Uint32 Random();
    void   SetTech(Uint32 techchoices);
    int    TechSlotsUsed(Peer & peer);
    void   SendMapDownloaded();
    void   PutMapChunk(Uint32 offset, Peer & peer);
    void   GetMapChunk(Uint32 offset);
    void   StoreMapChunk(unsigned char * data, Uint32 offset, Uint32 size);
    bool   SecurityIDCanSpawn(Uint8 securityid);

    // World state (public members)
    Input  localinput;
    Map    map;
    Resources resources;
    Audio & audio;
    Lobby  lobby;
    Uint8  gravity, maxyvelocity;
    int    minwalldistance;
    bool   replaying;
    Uint8  quitstate;
    std::vector<BuyableItem *> buyableitems;
    Uint32 tickcount;
    bool   choosingtech;

    // Debug
    bool debugoverlay;
    struct DebugLine { int x1, y1, x2, y2; Uint8 color; };
    std::vector<DebugLine> debuglines;

    // Systems
    TriggerGraph triggerGraph;
    bool         input_locked      = false;
    bool         player_spawn_emitted = false;
    int          secretsBeamed     = 0;
    GameMode *   gameMode          = nullptr;
    bool         matchEndCalled    = false;

    // Friends
    friend class Renderer;        friend class Game;
    friend class GameRenderer;    friend class GameInput;
    friend class GameUiPipeline;  friend class GameSession;
    friend class WorldMessaging;  friend class WorldObjectRegistry;
    friend class WorldNetwork;    friend class WorldPeerRegistry;
    friend class WorldReplication;
    friend class MapDownloader;   friend class AmbienceMixer;
    friend class Team;            friend class Lobby;
    friend class Player;          friend class Map;
    friend class RocketProjectile; friend class DedicatedServer;
    friend class SurveillanceMonitor; friend class Magistrate;
    friend class Vanta;           friend class GameStateObject;
    friend class DataRetrievalMode; friend class Warper;
    friend class Grenade;         friend class BaseDoor;
    friend class Terminal;        friend class PlayerAI;
    friend class Replay;          friend class Audio;
    friend class TriggerGraph;    friend class LobbyScreen;

    protected:
    void   SaveSnapshot(Serializer & data, Uint8 peerid)                                    { replication.SaveSnapshot(data, peerid); }
    void   LoadSnapshot(Serializer & data, bool create = true, Serializer * delta = 0, Uint16 objectid = 0) { replication.LoadSnapshot(data, create, delta, objectid); }
    Peer * AddPeer(char * address, unsigned short port, Uint8 agency, Uint32 accountid, Uint32 selectedcharid = 0, bool observer = false) { return peers.AddPeer(address, port, agency, accountid, selectedcharid, observer); }
    Peer * AddBot(Uint8 agency)                                                              { return peers.AddBot(agency); }

    char              mapname[256];
    std::vector<Uint32> ingameusers;

    private:
    void DoNetwork_Authority()                           { network.DoNetwork_Authority(); }
    void DoNetwork_Replica()                             { network.DoNetwork_Replica(); }
    Peer * FindPeer(sockaddr_in & sockaddr)              { return peers.FindPeer(sockaddr); }
    bool ProcessInputQueue(Peer & peer)                  { return replication.ProcessInputQueue(peer); }
    void ProcessSnapshotQueue()                          { replication.ProcessSnapshotQueue(); }
    void ClientSidePredict(Uint32 ourtick)               { replication.ClientSidePredict(ourtick); }
    void CheckExists()                                   { replication.CheckExists(); }
    void ClearSnapshotQueue()                            { replication.ClearSnapshotQueue(); }
    void ClearMapData();
    void AllocateMapData(int size);
    void LoadMapData(const char * filename);
    void SendGameInfo(Uint8 peerid)                      { replication.SendGameInfo(peerid); }
    void SendGameInfoLoaded()                            { replication.SendGameInfoLoaded(); }
    void SendReady()                                     { replication.SendReady(); }
    bool AllPeersReady(Uint8 except)                     { return replication.AllPeersReady(except); }
    bool AllPeersLoadedGameInfo()                        { return replication.AllPeersLoadedGameInfo(); }
    bool AllPeersDownloadedMap()                         { return replication.AllPeersDownloadedMap(); }
    char * CreateStatusString(const char * status, Uint8 color = 0, Uint8 duration = 100)   { return messaging.CreateStatusString(status, color, duration); }
    void   PushStatusString(char * statusstring)         { messaging.PushStatusString(statusstring); }
    void   RequestPeerList()                             { peers.RequestPeerList(); }
    void   SendSnapshots()                               { replication.SendSnapshots(); }
    void   SendPeerList(Uint8 peerid = 0)                { peers.SendPeerList(peerid); }
    void   ReadPeerList(Serializer & data)               { peers.ReadPeerList(data); }
    void   SendPacket(Peer * peer, char * data, unsigned int size) { network.SendPacket(peer, data, size); }
    void   SwitchToMode(bool newmode)                    { network.SwitchToMode(newmode); }
    void   DeleteOldSnapshots(Uint8 peerid)              { replication.DeleteOldSnapshots(peerid); }
    void   HandleDisconnect(Uint8 peerid, bool permanent = false) { peers.HandleDisconnect(peerid, permanent); }
    bool   RelevantToPlayer(Player * player, Object * object);
    bool   BelongsToTeam(Object & object, Uint16 teamid);
    void   ActivateTerminals();
    void   LoadBuyableItems();
    void   BuyItem(Uint8 id);
    void   RepairItem(Uint8 id);
    void   VirusItem(Uint8 id);
    void   ChangeTeam(Uint8 peerid);
    void   SetTech(Uint8 peerid, Uint32 techchoices);
    void   DisplayChatMessage(Uint32 accountid, const char * msg) { messaging.DisplayChatMessage(accountid, msg); }
    void   SendStats(Peer & peer)                        { peers.SendStats(peer); }
    void   UserInfoReceived(Peer & peer)                 { peers.UserInfoReceived(peer); }
    void   ApplyWantedTech(Peer & peer);
    bool   IsCollidable(Uint8 type);
    static bool CompareTeamByNumber(Team * t1, Team * t2)           { return WorldPeerRegistry::CompareTeamByNumber(t1, t2); }
    static bool CompareSnapshot(Serializer * s1, Serializer * s2)   { return WorldReplication::CompareSnapshot(s1, s2); }

    bool mode;
    static const unsigned int maxobjects         = WorldObjectRegistry::maxobjects;
    static const unsigned int maxpeers           = WorldPeerRegistry::maxpeers;
    static const unsigned int maxoldsnapshots    = WorldReplication::maxoldsnapshots;
    static const unsigned int maxlocalinputhistory = WorldReplication::maxlocalinputhistory;
    static const unsigned int peertimeout        = WorldPeerRegistry::peertimeout;
    static const unsigned int maxteams           = WorldPeerRegistry::maxteams;

    char version[16];

    enum { IDLE, LISTENING, CONNECTING, CONNECTED };
    enum { NONE, INLOBBY, INGAME } gameplaystate;
    enum { MSG_CONNECT, MSG_SNAPSHOT, MSG_INPUT, MSG_PEERLIST, MSG_DISCONNECT,
        MSG_PING, MSG_PONG, MSG_GAMEINFO, MSG_READY, MSG_CHAT, MSG_STATION,
        MSG_CHANGETEAM, MSG_STATUS, MSG_MESSAGE, MSG_GOVTKILL, MSG_SOUND,
        MSG_TECH, MSG_STATS, MSG_EXISTS, MSG_REMOVE, MSG_MAP, MSG_SETAGENCY,
        MSG_KICK, MSG_TRIGGER_STATE, MSG_CAMERA };
    enum { STA_BUY, STA_REPAIR, STA_VIRUS };
    enum { MAP_DOWNLOADED, MAP_GETCHUNK, MAP_PUTCHUNK };

    ObjectTypes objecttypes;
    bool        systemcameraactive[2];
    Uint16      systemcamerafollow[2];
    Sint16      systemcamerax[2], systemcameray[2];
    DedicatedServer dedicatedserver;
    LobbyGame   gameinfo;
    Uint16      winningteamid;
    bool        highlightsecrets, highlightminimap, intutorialmode;
    Uint32      randomseed;
    Replay      replay;
    std::vector<unsigned char> currentmapdata;
    bool        currentmapdataprocessed, currentmapdataend;
    bool        showteamcolors, showplayerlist;
};

#endif
