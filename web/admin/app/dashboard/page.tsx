'use client';
import { useState } from 'react';
import { useAuth } from '../../lib/auth';
import { useSocket } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import StatCard from '../../components/StatCard';

interface PlayerState {
  name: string;
  gameId: number | string;
  gameStatus: number;
}

interface GameState {
  name?: string;
  mapName?: string;
  state: string;
  accountId?: string;
  hostname?: string;
  port?: number;
  lastHeartbeat?: number;
  tickCount?: number;
  aliveMask?: number;
}

const STATE_LABELS: Record<string, string> = { created: 'STARTING', ready: 'IN ROOM', playing: 'IN GAME' };
const STATE_COLORS: Record<string, string> = { created: 'text-game-warning', ready: 'text-game-primary', playing: 'text-game-info' };

function HeartbeatBadge({ lastHeartbeat }: { lastHeartbeat?: number }) {
  if (!lastHeartbeat) return <span className="text-xs font-mono text-game-muted">NO HB</span>;
  const ageMs = Date.now() - lastHeartbeat;
  const ageSec = Math.floor(ageMs / 1000);
  const color = ageMs < 5000 ? 'text-green-400' : ageMs < 15000 ? 'text-yellow-400' : 'text-red-400';
  const dot = ageMs < 5000 ? '●' : ageMs < 15000 ? '◐' : '○';
  return <span className={`text-xs font-mono ${color}`}>{dot} {ageSec}s</span>;
}

export default function Dashboard() {
  useAuth();
  const [players, setPlayers] = useState<Record<string, PlayerState>>({});
  const [games,   setGames]   = useState<Record<string, GameState>>({});
  const [expanded, setExpanded] = useState<Record<string, boolean>>({});

  const wsConnected = useSocket({
    snapshot: (...args: unknown[]) => {
      const d = args[0] as { players?: Record<string, PlayerState>; games?: Record<string, GameState> };
      setPlayers(d.players || {});
      setGames(d.games || {});
    },
    'player.login':   (...args: unknown[]) => {
      const d = args[0] as { accountId: string; name: string };
      setPlayers(prev => ({ ...prev, [d.accountId]: { name: d.name, gameId: 0, gameStatus: 0 } }));
    },
    'player.logout':  (...args: unknown[]) => {
      const d = args[0] as { accountId: string };
      setPlayers(prev => { const n = { ...prev }; delete n[d.accountId]; return n; });
    },
    'player.presence': (...args: unknown[]) => {
      const d = args[0] as { accountId: string; gameId: number; gameStatus: number };
      setPlayers(prev => ({
        ...prev, [d.accountId]: { ...prev[d.accountId], gameId: d.gameId, gameStatus: d.gameStatus }
      }));
    },
    'game.created': (...args: unknown[]) => {
      const d = args[0] as { gameId: string; name: string; mapName: string; accountId: string };
      setGames(prev => ({ ...prev, [d.gameId]: { name: d.name, mapName: d.mapName, state: 'created', accountId: d.accountId } }));
    },
    'game.ready': (...args: unknown[]) => {
      const d = args[0] as { gameId: string; hostname: string; port: number };
      setGames(prev => ({ ...prev, [d.gameId]: { ...prev[d.gameId], hostname: d.hostname, port: d.port, state: 'ready' } }));
    },
    'game.heartbeat': (...args: unknown[]) => {
      const d = args[0] as { gameId: string; ts: number; tickCount: number; aliveMask: number };
      setGames(prev => {
        if (!prev[d.gameId]) return prev;
        return { ...prev, [d.gameId]: { ...prev[d.gameId], lastHeartbeat: d.ts, tickCount: d.tickCount, aliveMask: d.aliveMask } };
      });
    },
    'game.ended': (...args: unknown[]) => {
      const d = args[0] as { gameId: string };
      setGames(prev => { const n = { ...prev }; delete n[d.gameId]; return n; });
      setExpanded(prev => { const n = { ...prev }; delete n[d.gameId]; return n; });
    },
  });

  const playerList = Object.entries(players);
  const gameList   = Object.entries(games);

  const toggleExpand = (gid: string) =>
    setExpanded(prev => ({ ...prev, [gid]: !prev[gid] }));

  const playersInGame = (gid: string) =>
    playerList.filter(([, p]) => String(p.gameId) === gid);

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <h1 className="text-game-primary font-mono text-xl tracking-widest mb-6">◉ LIVE SESSIONS</h1>

        {/* Stats row */}
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
          <StatCard label="ONLINE PLAYERS" value={playerList.length} color="primary" />
          <StatCard label="ACTIVE GAMES"   value={gameList.length}   color="info" />
          <StatCard label="IN GAME"         value={playerList.filter(([, p]) => p.gameStatus === 2).length} color="warning" />
          <StatCard label="IN ROOM"         value={playerList.filter(([, p]) => p.gameStatus === 1).length} color="primary" />
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Active Games */}
          <section>
            <h2 className="text-xs tracking-widest text-game-textDim mb-3">ACTIVE GAMES</h2>
            {gameList.length === 0
              ? <div className="text-game-muted text-xs py-8 text-center border border-game-border rounded">NO ACTIVE GAMES</div>
              : gameList.map(([gid, g]) => {
                  const inGame = playersInGame(gid);
                  const isExpanded = expanded[gid];
                  return (
                    <div key={gid} className="bg-game-bgCard border border-game-border rounded mb-3 overflow-hidden">
                      <button
                        className="w-full text-left p-4 hover:bg-game-bgHover transition-colors"
                        onClick={() => toggleExpand(gid)}
                      >
                        <div className="flex justify-between items-start">
                          <div>
                            <div className="font-mono text-game-text text-sm">{g.name || `GAME #${gid}`}</div>
                            <div className="text-xs text-game-textDim mt-0.5">{g.mapName}</div>
                          </div>
                          <div className="flex items-center gap-3">
                            <HeartbeatBadge lastHeartbeat={g.lastHeartbeat} />
                            <span className={`text-xs font-mono ${STATE_COLORS[g.state] || 'text-game-muted'}`}>
                              {STATE_LABELS[g.state] || g.state?.toUpperCase()}
                            </span>
                            <span className="text-game-muted text-xs">{isExpanded ? '▲' : '▼'}</span>
                          </div>
                        </div>
                        {g.hostname && (
                          <div className="text-xs text-game-muted mt-2">{g.hostname}:{g.port}</div>
                        )}
                      </button>

                      {isExpanded && (
                        <div className="border-t border-game-border px-4 pb-3 pt-2">
                          {g.tickCount !== undefined && (
                            <div className="text-xs text-game-textDim mb-2">TICK {g.tickCount}</div>
                          )}
                          {inGame.length === 0
                            ? <div className="text-xs text-game-muted py-2">NO PLAYERS IN THIS GAME</div>
                            : (
                              <table className="w-full text-xs font-mono">
                                <thead>
                                  <tr className="text-game-textDim">
                                    <th className="text-left py-1">PLAYER</th>
                                    <th className="text-left py-1">STATUS</th>
                                  </tr>
                                </thead>
                                <tbody>
                                  {inGame.map(([aid, p]) => (
                                    <tr key={aid} className="border-t border-game-border">
                                      <td className="py-1 text-game-text">{p.name}</td>
                                      <td className={`py-1 ${p.gameStatus === 2 ? 'text-game-warning' : 'text-game-primary'}`}>
                                        {p.gameStatus === 2 ? 'PLAYING' : 'IN ROOM'}
                                      </td>
                                    </tr>
                                  ))}
                                </tbody>
                              </table>
                            )
                          }
                        </div>
                      )}
                    </div>
                  );
                })
            }
          </section>

          {/* Online Players */}
          <section>
            <h2 className="text-xs tracking-widest text-game-textDim mb-3">ONLINE PLAYERS</h2>
            {playerList.length === 0
              ? <div className="text-game-muted text-xs py-8 text-center border border-game-border rounded">NO PLAYERS ONLINE</div>
              : (
                <div className="bg-game-bgCard border border-game-border rounded overflow-hidden">
                  <table className="w-full text-xs font-mono">
                    <thead>
                      <tr className="border-b border-game-border text-game-textDim">
                        <th className="text-left px-4 py-2">PLAYER</th>
                        <th className="text-left px-4 py-2">STATUS</th>
                        <th className="text-left px-4 py-2">GAME</th>
                      </tr>
                    </thead>
                    <tbody>
                      {playerList.map(([aid, p]) => (
                        <tr key={aid} className="border-b border-game-border last:border-0 hover:bg-game-bgHover">
                          <td className="px-4 py-2 text-game-text">{p.name}</td>
                          <td className={`px-4 py-2 ${p.gameStatus === 2 ? 'text-game-warning' : p.gameStatus === 1 ? 'text-game-primary' : 'text-game-textDim'}`}>
                            {p.gameStatus === 2 ? 'PLAYING' : p.gameStatus === 1 ? 'IN ROOM' : 'IN LOBBY'}
                          </td>
                          <td className="px-4 py-2 text-game-muted">{p.gameId || '—'}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )
            }
          </section>
        </div>
      </main>
    </div>
  );
}
