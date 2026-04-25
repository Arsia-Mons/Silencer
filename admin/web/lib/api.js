// Thin wrapper around fetch with auth header injection
export const API = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080';

function getToken() {
  if (typeof window === 'undefined') return null;
  return localStorage.getItem('zs_token');
}

function getPlayerToken() {
  if (typeof window === 'undefined') return null;
  return localStorage.getItem('zs_player_token');
}

export async function apiFetch(path, opts = {}, tokenOverride) {
  const token = tokenOverride !== undefined ? tokenOverride : getToken();
  const res = await fetch(`${API}${path}`, {
    ...opts,
    headers: {
      'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...opts.headers,
    },
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

function playerFetch(path, opts = {}) {
  return apiFetch(path, opts, getPlayerToken());
}

export const login = (username, password) =>
  apiFetch('/auth/login', { method: 'POST', body: JSON.stringify({ username, password }) }, null);

export const playerLogin = (username, password) =>
  apiFetch('/auth/player-login', { method: 'POST', body: JSON.stringify({ username, password }) }, null);

export const getMyProfile = ()              => playerFetch('/me');
export const getMyMatches = (page = 1)     => playerFetch(`/me/matches?page=${page}&limit=20`);

export const getPlayers   = (params = {}) => apiFetch('/players?'    + new URLSearchParams(params));
export const getPlayer    = (id)          => apiFetch(`/players/${id}`);
export const getPlayerMatches = (id, page = 1) => apiFetch(`/players/${id}/matches?page=${page}&limit=20`);
export const banPlayer    = (id, banned, reason) => apiFetch(`/players/${id}/ban`, { method: 'PATCH', body: JSON.stringify({ banned, reason }) });
export const deletePlayer = (id)          => apiFetch(`/players/${id}`, { method: 'DELETE' });
export const getSessions  = (params = {}) => apiFetch('/sessions?'   + new URLSearchParams(params));
export const getEvents    = (params = {}) => apiFetch('/events?'     + new URLSearchParams(params));
export const getStats     = ()            => apiFetch('/stats');
export const getAdminUsers      = ()             => apiFetch('/auth/users');
export const createAdminUser    = (data)         => apiFetch('/auth/users', { method: 'POST', body: JSON.stringify(data) });
export const updateAdminUser    = (id, data)     => apiFetch(`/auth/users/${id}`, { method: 'PATCH', body: JSON.stringify(data) });
export const resetAdminPassword = (id, password) => apiFetch(`/auth/users/${id}/password`, { method: 'PATCH', body: JSON.stringify({ password }) });
export const deleteAdminUser    = (id)           => apiFetch(`/auth/users/${id}`, { method: 'DELETE' });
export const changeOwnPassword  = (currentPassword, newPassword) =>
  apiFetch('/auth/me/password', { method: 'PATCH', body: JSON.stringify({ currentPassword, newPassword }) });

export const triggerBackup  = ()  => apiFetch('/backup/trigger', { method: 'POST' });
export const getBackupStatus = () => apiFetch('/backup/status');
export const listBackups    = ()  => apiFetch('/backup/list');

export const getGameStatsRecent      = (limit = 20) => playerFetch(`/gamestats/recent-games?limit=${limit}`);
export const getGameStatsLeaderboard = (limit = 50) => playerFetch(`/gamestats/leaderboard?limit=${limit}`);

