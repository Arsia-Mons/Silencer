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

export interface NavLink { fromIdx: number; toIdx: number; type: 0 | 1 | 2; sourceX: number; targetX: number; }

// ── Trigger scripting types (issue #30) ─────────────────────────────────────

export type TriggerEventType =
  | 'NONE' | 'TRIGGER_ENTER_ZONE' | 'TERMINAL_ACTIVATED' | 'ACTOR_KILLED'
  | 'ACTOR_DAMAGED' | 'OBJECTIVE_COMPLETE' | 'ITEM_COLLECTED' | 'PLAYER_DIED'
  | 'ALL_PLAYERS_DIED' | 'TIMER_EXPIRED' | 'GAME_START' | 'PLAYER_SPAWN';

export type TriggerActionType =
  | 'NONE' | 'OPEN_DOOR' | 'LOCK_DOOR' | 'UNLOCK_DOOR' | 'PLAY_SOUND'
  | 'SHOW_OBJECTIVE' | 'PAN_CAMERA' | 'SPAWN_ACTOR' | 'END_MISSION'
  | 'DESTROY_ACTOR' | 'MOVE_ACTOR' | 'APPLY_DAMAGE_IN_ZONE'
  | 'ENABLE_TRIGGER' | 'DISABLE_TRIGGER'
  | 'COMPLETE_OBJECTIVE' | 'LOCK_INPUT' | 'UNLOCK_INPUT' | 'SET_FLAG';

export type TriggerConditionType =
  | 'NONE' | 'TEAM_CHECK' | 'OBJECTIVE_STATE' | 'PLAYER_COUNT' | 'HEALTH_THRESHOLD'
  | 'COUNT_REACHED' | 'FLAG_SET';

export type ConditionLogic = 'ALL_OF' | 'ANY_OF';

export interface TriggerCondition {
  type: TriggerConditionType;
  team: number;
  objectiveId: number;
  playerCount: number;
  healthPct: number;
  count: number;  // COUNT_REACHED: minimum hit count
}

export interface TriggerAction {
  type: TriggerActionType;
  actorId: number;
  delay: number;       // seconds
  paramX: number;
  paramY: number;
  paramF: number;
  paramU8: number;
  sound: string;       // max 63 chars
  message: string;     // max 127 chars
}

export interface TriggerNode {
  id: number;
  triggerEvent: TriggerEventType;
  actorId: number;      // 0 = any actor
  oneShot: boolean;
  enabled: boolean;
  conditionLogic: ConditionLogic;
  timerSeconds: number; // > 0 for TIMER_EXPIRED nodes
  conditions: TriggerCondition[];
  actions: TriggerAction[];
}

export interface ObjectiveDef {
  id: number;
  required: boolean;
  text: string;         // max 127 chars
}

export interface TriggerZone {
  id: number;
  label: string;
  x1: number;
  y1: number;
  x2: number;
  y2: number;
}

export interface SilMapData {
  header: MapHeader;
  width: number;
  height: number;
  layers: MapLayers;
  actors: MapActor[];
  platforms: MapPlatform[];
  shadowZones: MapShadowZone[];
  navLinks: NavLink[];
  triggers: TriggerNode[];
  objectives: ObjectiveDef[];
  zones?: TriggerZone[];
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
