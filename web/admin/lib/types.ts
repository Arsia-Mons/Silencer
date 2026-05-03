export interface Agency {
  level: number;
  xp: number;
  wins: number;
  losses: number;
  endurance: number;
  shield: number;
  jetpack: number;
  techSlots: number;
  hacking: number;
  contacts: number;
  xpToNextLevel?: number;
}

export interface LifetimeStats {
  kills: number;
  deaths: number;
  wins: number;
  losses: number;
  playtime: number;
  [key: string]: number;
}

export interface Player {
  accountId: string;
  name: string;
  banned: boolean;
  banReason?: string;
  rank: string;
  agencies: Agency[];
  lifetimeStats: LifetimeStats;
  ipHistory: string[];
  loginCount?: number;
  firstSeen?: string;
  lastSeen?: string;
  totalPlaytimeSecs?: number;
}

export interface MatchStat {
  gameId: string;
  accountId: string;
  won: boolean;
  agency: string;
  kills: number;
  deaths: number;
  credits: number;
  playedAt: string;
  _id?: string;
  createdAt?: string;
  xp?: number;
  filesHacked?: number;
  secretsReturned?: number;
  team?: number;
  win?: boolean;
}

export interface AdminUser {
  _id: string;
  id: string;
  username: string;
  role: string;
  rank: string;
  createdAt: string;
  createdBy?: string;
}

export interface LobbyGame {
  id: string;
  name: string;
  mapname: string;
  players: number;
  maxplayers: number;
  status: string;
  hostname: string;
  port: number;
}

export interface AuditEvent {
  _id: string;
  type: string;
  data: unknown;
  createdAt: string;
  ts?: Date | string;
  accountId?: string;
  gameId?: string;
  name?: string;
}

export interface BackupInfo {
  filename: string;
  sizeKB: number;
  size: number;
  ts?: string;
  createdAt: string;
  githubUrl?: string;
}

export interface BackupResult {
  ok: boolean;
  filename?: string;
  sizeKB?: number;
  ts?: string;
  githubUrl?: string;
  githubError?: string;
  error?: string;
}

export interface BackupStatus {
  inProgress: boolean;
  githubConfigured?: boolean;
  lastResult?: BackupResult;
}

export interface StatsSnapshot {
  onlinePlayers: number;
  activeGames: number;
  totalPlayers: number;
  totalGames: number;
  lobby: {
    onlinePlayers: number;
    activeGames: number;
  };
  db: {
    totalPlayers: number;
    totalEvents: number;
    status: string;
  };
  rabbitmq: {
    status: string;
  };
}

// Map designer types
export interface TileCell {
  tile_id: number;
  flip: number;
  lum: number;
}

export interface MapHeader {
  firstbyte: number;
  version: number;
  maxplayers: number;
  maxteams: number;
  parallax: number;
  ambience: number;
  flags: number;
  description: string;
}

export interface MapActor {
  id: number;
  x: number;
  y: number;
  direction: number;
  type: number;
  matchid: number;
  subplane: number;
  unknown: number;
  securityid: number;
}

export interface MapPlatform {
  x1: number;
  y1: number;
  x2: number;
  y2: number;
  type1: number;
  type2: number;
  typeName: string;
}

export interface MapLayers {
  bg: (TileCell | null)[][];
  fg: (TileCell | null)[][];
}

export interface MapShadowZone {
  x1: number;
  y1: number;
  x2: number;
  y2: number;
}

export interface NavLink { fromIdx: number; toIdx: number; type: 0 | 1 | 2; }

export interface SilMapData {
  header: MapHeader;
  width: number;
  height: number;
  layers: MapLayers;
  actors: MapActor[];
  platforms: MapPlatform[];
  shadowZones: MapShadowZone[];
  navLinks: NavLink[];
  rawMinimap: Uint8Array;
  minimapCompressedSize: number;
  fileName?: string;
}

export interface SpriteEntry {
  bitmap: ImageBitmap;
  offsetX: number;
  offsetY: number;
  width: number;
  height: number;
}

export interface ActorDef {
  id: number;
  label: string;
  icon: string;
  color: string;
  bank: number | null;
  frame: number;
}

export interface Platform {
  x: number;
  y: number;
  w: number;
  h: number;
  type: string;
  [key: string]: unknown;
}

export interface Actor {
  id: string;
  type: string;
  x: number;
  y: number;
  [key: string]: unknown;
}

export interface MapLayer {
  id: string;
  tiles: number[][];
  [key: string]: unknown;
}

export interface SilMap {
  name: string;
  width: number;
  height: number;
  platforms: Platform[];
  actors: Actor[];
  layers: MapLayer[];
  [key: string]: unknown;
}
