// Wire-protocol types mirrored from services/lobby/protocol.go.
// See ../../../shared/lobby-protocol/protocol.md.

// Opcodes — must match services/lobby/protocol.go.
export const Op = {
    Auth: 0,
    MOTD: 1,
    Chat: 2,
    NewGame: 3,
    DelGame: 4,
    Channel: 5,
    Connect: 6,
    Version: 7,
    UserInfo: 8,
    Ping: 9,
    UpgradeStat: 10,
    RegisterStats: 11,
    Presence: 12,
    SetGame: 13,
} as const;
export type Op = (typeof Op)[keyof typeof Op];

export const Platform = {
    Unknown: 0,
    MacOSARM64: 1,
    WindowsX64: 2,
} as const;
export type Platform = (typeof Platform)[keyof typeof Platform];

export const SecurityLevel = {
    None: 0,
    Low: 1,
    Medium: 2,
    High: 3,
} as const;
export type SecurityLevel = (typeof SecurityLevel)[keyof typeof SecurityLevel];

export const GameStatus = {
    Lobby: 0,
    Pregame: 1,
    Playing: 2,
} as const;
export type GameStatus = (typeof GameStatus)[keyof typeof GameStatus];

export interface AgencyStats {
    wins: number;
    losses: number;
    xpToNextLevel: number;
    level: number;
    endurance: number;
    shield: number;
    jetpack: number;
    techSlots: number;
    hacking: number;
    contacts: number;
}

export const emptyAgency = (): AgencyStats => ({
    wins: 0,
    losses: 0,
    xpToNextLevel: 0,
    level: 0,
    endurance: 0,
    shield: 0,
    jetpack: 0,
    techSlots: 0,
    hacking: 0,
    contacts: 0,
});

export interface UserInfo {
    accountId: number;
    agencies: [AgencyStats, AgencyStats, AgencyStats, AgencyStats, AgencyStats];
    name: string;
}

export interface LobbyGame {
    id: number;
    accountId: number; // host
    name: string;
    password: string;
    hostname: string; // "ip,port"
    mapName: string;
    mapHash: Uint8Array; // 20 bytes
    players: number;
    state: number;
    securityLevel: SecurityLevel;
    minLevel: number;
    maxLevel: number;
    maxPlayers: number;
    maxTeams: number;
    extra: number;
    port: number;
}

export interface WeaponStats {
    fires: number;
    hits: number;
    playerKills: number;
}

// 44 × u32 LE = 176 bytes. Matches services/lobby/client.go::handleRegisterStats.
export interface MatchStats {
    weapons: [WeaponStats, WeaponStats, WeaponStats, WeaponStats];
    civiliansKilled: number;
    guardsKilled: number;
    robotsKilled: number;
    defenseKilled: number;
    secretsPickedUp: number;
    secretsReturned: number;
    secretsStolen: number;
    secretsDropped: number;
    powerupsPickedUp: number;
    deaths: number;
    kills: number;
    suicides: number;
    poisons: number;
    tractsPlanted: number;
    grenadesThrown: number;
    neutronsThrown: number;
    empsThrown: number;
    shapedThrown: number;
    plasmasThrown: number;
    flaresThrown: number;
    poisonFlaresThrown: number;
    healthPacksUsed: number;
    fixedCannonsPlaced: number;
    fixedCannonsDestroyed: number;
    detsPlanted: number;
    camerasPlanted: number;
    virusesUsed: number;
    filesHacked: number;
    filesReturned: number;
    creditsEarned: number;
    creditsSpent: number;
    healsDone: number;
}

export const emptyMatchStats = (): MatchStats => ({
    weapons: [
        { fires: 0, hits: 0, playerKills: 0 },
        { fires: 0, hits: 0, playerKills: 0 },
        { fires: 0, hits: 0, playerKills: 0 },
        { fires: 0, hits: 0, playerKills: 0 },
    ],
    civiliansKilled: 0,
    guardsKilled: 0,
    robotsKilled: 0,
    defenseKilled: 0,
    secretsPickedUp: 0,
    secretsReturned: 0,
    secretsStolen: 0,
    secretsDropped: 0,
    powerupsPickedUp: 0,
    deaths: 0,
    kills: 0,
    suicides: 0,
    poisons: 0,
    tractsPlanted: 0,
    grenadesThrown: 0,
    neutronsThrown: 0,
    empsThrown: 0,
    shapedThrown: 0,
    plasmasThrown: 0,
    flaresThrown: 0,
    poisonFlaresThrown: 0,
    healthPacksUsed: 0,
    fixedCannonsPlaced: 0,
    fixedCannonsDestroyed: 0,
    detsPlanted: 0,
    camerasPlanted: 0,
    virusesUsed: 0,
    filesHacked: 0,
    filesReturned: 0,
    creditsEarned: 0,
    creditsSpent: 0,
    healsDone: 0,
});

// ---- Inbound event shapes -----------------------------------------------

export interface AuthResult {
    ok: boolean;
    accountId: number;
    error: string;
}

export interface VersionResult {
    ok: boolean;
    updateUrl: string; // empty unless reject + update available
    sha256: Uint8Array; // 32 bytes; meaningful only when updateUrl !== ""
}

export interface ChatMessage {
    channel: string;
    text: string;
    color: number;
    brightness: number;
}

export interface PresenceUpdate {
    removed: boolean;
    accountId: number;
    gameId: number;
    status: GameStatus;
    name: string;
}

export interface NewGameEvent {
    status: number; // 1 = success/advertise, 2 = create failed
    game: LobbyGame;
}

export interface MotdChunk {
    text: string;
    terminator: boolean;
}
