// Thin wrapper around fetch with auth header injection.
//
// Production: NEXT_PUBLIC_API_URL is unset → API is "/api" (relative).
//   Cloudflare Tunnel routes /api/* on admin.arsiamons.com to admin-api:24080.
// Local dev: NEXT_PUBLIC_API_URL=http://localhost:24080 (compose build arg)
//   → API is "http://localhost:24080/api"; admin-api still serves under /api.
import type { Player, MatchStat, AdminUser, AuditEvent, BackupStatus, BackupInfo, StatsSnapshot } from './types';

export const API = (process.env.NEXT_PUBLIC_API_URL || '') + '/api';

function getToken(): string | null {
  if (typeof window === 'undefined') return null;
  return localStorage.getItem('zs_token');
}

function getPlayerToken(): string | null {
  if (typeof window === 'undefined') return null;
  return localStorage.getItem('zs_player_token');
}

export async function apiFetch(path: string, opts: RequestInit = {}, tokenOverride?: string | null): Promise<unknown> {
  const token = tokenOverride !== undefined ? tokenOverride : getToken();
  const res = await fetch(`${API}${path}`, {
    ...opts,
    headers: {
      'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...(opts.headers as Record<string, string> | undefined),
    },
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

function playerFetch(path: string, opts: RequestInit = {}): Promise<unknown> {
  return apiFetch(path, opts, getPlayerToken());
}

export const login = (username: string, password: string): Promise<unknown> =>
  apiFetch('/auth/login', { method: 'POST', body: JSON.stringify({ username, password }) }, null);

export const playerLogin = (username: string, password: string): Promise<unknown> =>
  apiFetch('/auth/player-login', { method: 'POST', body: JSON.stringify({ username, password }) }, null);

export const getMyProfile = (): Promise<unknown>              => playerFetch('/me');
export const getMyMatches = (page = 1): Promise<unknown>     => playerFetch(`/me/matches?page=${page}&limit=20`);

export const getPlayers   = (params: Record<string, unknown> = {}): Promise<unknown> => apiFetch('/players?'    + new URLSearchParams(params as Record<string, string>));
export const getPlayer    = (id: string): Promise<Player>    => apiFetch(`/players/${id}`) as Promise<Player>;
export const getPlayerMatches = (id: string, page = 1): Promise<{ matches: MatchStat[]; total: number }> =>
  apiFetch(`/players/${id}/matches?page=${page}&limit=20`) as Promise<{ matches: MatchStat[]; total: number }>;
export const banPlayer    = (id: string, banned: boolean, reason: string): Promise<unknown> => apiFetch(`/players/${id}/ban`, { method: 'PATCH', body: JSON.stringify({ banned, reason }) });
export const deletePlayer = (id: string): Promise<unknown>   => apiFetch(`/players/${id}`, { method: 'DELETE' });
export const getSessions  = (params: Record<string, unknown> = {}): Promise<unknown> => apiFetch('/sessions?'   + new URLSearchParams(params as Record<string, string>));
export const getEvents    = (params: Record<string, unknown> = {}): Promise<{ events: AuditEvent[]; total: number }> =>
  apiFetch('/events?' + new URLSearchParams(params as Record<string, string>)) as Promise<{ events: AuditEvent[]; total: number }>;
export const getStats     = (): Promise<StatsSnapshot>       => apiFetch('/stats') as Promise<StatsSnapshot>;
export const getAdminUsers      = (): Promise<AdminUser[]>          => apiFetch('/auth/users') as Promise<AdminUser[]>;
export const createAdminUser    = (data: unknown): Promise<unknown> => apiFetch('/auth/users', { method: 'POST', body: JSON.stringify(data) });
export const updateAdminUser    = (id: string, data: unknown): Promise<unknown> => apiFetch(`/auth/users/${id}`, { method: 'PATCH', body: JSON.stringify(data) });
export const resetAdminPassword = (id: string, password: string): Promise<unknown> => apiFetch(`/auth/users/${id}/password`, { method: 'PATCH', body: JSON.stringify({ password }) });
export const deleteAdminUser    = (id: string): Promise<unknown>    => apiFetch(`/auth/users/${id}`, { method: 'DELETE' });
export const changeOwnPassword  = (currentPassword: string, newPassword: string): Promise<unknown> =>
  apiFetch('/auth/me/password', { method: 'PATCH', body: JSON.stringify({ currentPassword, newPassword }) });

export const triggerBackup  = (): Promise<unknown>          => apiFetch('/backup/trigger', { method: 'POST' });
export const getBackupStatus = (): Promise<BackupStatus>    => apiFetch('/backup/status') as Promise<BackupStatus>;
export const listBackups    = (): Promise<{ files: BackupInfo[] }> => apiFetch('/backup/list') as Promise<{ files: BackupInfo[] }>;

export const getGameStatsRecent      = (limit = 20): Promise<unknown> => playerFetch(`/gamestats/recent-games?limit=${limit}`);
export const getGameStatsLeaderboard = (limit = 50): Promise<unknown> => playerFetch(`/gamestats/leaderboard?limit=${limit}`);
export const getGameDetail           = (gameId: string): Promise<unknown> => playerFetch(`/gamestats/game/${gameId}`);
export const getAgentDetail          = (accountId: string): Promise<unknown> => playerFetch(`/gamestats/player/${accountId}`);

// Sprite endpoints
export type BankInfo = { bank: number; frames: number };
export type FrameMeta = { frame: number; width: number; height: number; offsetX: number; offsetY: number };

export const getSpriteBanks   = (): Promise<BankInfo[]>    => apiFetch('/sprites') as Promise<BankInfo[]>;
export const getSpriteFrames  = (bank: number): Promise<FrameMeta[]> => apiFetch(`/sprites/${bank}/frames`) as Promise<FrameMeta[]>;
/** Returns a URL usable in <img src=…> that goes through the Next.js proxy with auth injected by the browser. */
export function spriteUrl(bank: number, frame: number): string {
  const token = getToken();
  return `/api/sprites/${bank}/${frame}?_t=${token ? token.slice(-8) : ''}`;
}
/** Fetch sprite PNG as a blob URL (use when img tag proxy isn't available). */
export async function fetchSpriteBlob(bank: number, frame: number): Promise<string> {
  const token = getToken();
  const res = await fetch(`${API}/sprites/${bank}/${frame}`, {
    headers: token ? { Authorization: `Bearer ${token}` } : {},
  });
  if (!res.ok) throw new Error(`${res.status}`);
  const blob = await res.blob();
  return URL.createObjectURL(blob);
}

// Actor CRUD endpoints
export interface StateMachineTransition {
  id: string;
  from: string;
  to: string;
  condition: string;
}

export interface StateMachine {
  initial: string | null;
  transitions: StateMachineTransition[];
  positions: Record<string, { x: number; y: number }>;
}

export interface FrameDef {
  bank: number;
  index: number;
  duration: number;
  hurtbox?: [number, number, number, number];
}

export interface AnimSequence {
  loop: boolean;
  frames: FrameDef[];
}

export interface ActorProps {
  hp: number;
  shield: number;
  speed: number;
  faction: string;
  spawnWeight: number;
}

export interface ActorDef {
  id?: string;
  props?: ActorProps;
  sequences?: Record<string, AnimSequence>;
  stateMachine?: StateMachine;
  behaviortreeId?: string;
  [key: string]: unknown;
}

export const listActors   = (): Promise<string[]>               => apiFetch('/actors') as Promise<string[]>;
export const getActor     = (id: string): Promise<ActorDef>     => apiFetch(`/actors/${id}`) as Promise<ActorDef>;
export const saveActor    = (id: string, def: ActorDef): Promise<unknown> =>
  apiFetch(`/actors/${id}`, { method: 'PUT', body: JSON.stringify(def) });
export const deleteActor  = (id: string): Promise<unknown>      => apiFetch(`/actors/${id}`, { method: 'DELETE' });

// Behavior Tree types + CRUD
export type BTNodeType =
  | 'Selector' | 'Sequence' | 'Parallel' | 'RandomSelector'
  | 'Inverter' | 'Cooldown' | 'Repeat' | 'Timeout' | 'ForceSuccess'
  | 'Wait' | 'Leaf' | 'Condition';

export interface BTNode {
  type: BTNodeType;
  label: string;
  children: string[];
  props: Record<string, unknown>;
}

export interface BBKey {
  key: string;
  type: 'bool' | 'int' | 'float' | 'string';
  default: unknown;
}

export interface BehaviorTree {
  version: number;
  id: string;
  blackboard: BBKey[];
  rootId: string;
  nodes: Record<string, BTNode>;
  positions: Record<string, { x: number; y: number }>;
}

export const listBehaviorTrees = (): Promise<string[]> =>
  apiFetch('/behaviortrees') as Promise<string[]>;
export const getBehaviorTree = (id: string): Promise<BehaviorTree> =>
  apiFetch(`/behaviortrees/${id}`) as Promise<BehaviorTree>;
export const saveBehaviorTree = (id: string, bt: BehaviorTree): Promise<unknown> =>
  apiFetch(`/behaviortrees/${id}`, { method: 'PUT', body: JSON.stringify(bt) });
export const deleteBehaviorTree = (id: string): Promise<unknown> =>
  apiFetch(`/behaviortrees/${id}`, { method: 'DELETE' });
