'use client';
import { useState } from 'react';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import StatCard from '../../components/StatCard.js';

const STATE_LABELS = { created: 'STARTING', ready: 'IN ROOM', playing: 'IN GAME' };
const STATE_COLORS = { created: 'text-game-warning', ready: 'text-game-primary', playing: 'text-game-info' };

export default function Dashboard() {
  useAuth();
  const [players, setPlayers] = useState({});
  const [games,   setGames]   = useState({});

  const wsConnected = useSocket({
    snapshot: ({ players: p, games: g }) => {
      setPlayers(p || {});
      setGames(g || {});
    },
    'player.login':   (d) => setPlayers(prev => ({ ...prev, [d.accountId]: { name: d.name, gameId: 0, gameStatus: 0 } })),
    'player.logout':  (d) => setPlayers(prev => { const n = { ...prev }; delete n[d.accountId]; return n; }),
    'player.presence':(d) => setPlayers(prev => ({
      ...prev, [d.accountId]: { ...prev[d.accountId], gameId: d.gameId, gameStatus: d.gameStatus }
    })),
    'game.created': (d) => setGames(prev => ({ ...prev, [d.gameId]: { name: d.name, mapName: d.mapName, state: 'created', accountId: d.accountId } })),
    'game.ready':   (d) => setGames(prev => ({ ...prev, [d.gameId]: { ...prev[d.gameId], hostname: d.hostname, port: d.port, state: 'ready' } })),
    'game.ended':   (d) => setGames(prev => { const n = { ...prev }; delete n[d.gameId]; return n; }),
  });

  const playerList = Object.entries(players);
  const gameList   = Object.entries(games);

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <h1 className="text-game-primary font-mono text-xl tracking-widest mb-6">◉ LIVE SESSIONS</h1>

        {/* Stats row */}
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
          <StatCard label="ONLINE PLAYERS" value={playerList.length} color="primary" />
          <StatCard label="ACTIVE GAMES"   value={gameList.length}   color="info" />
          <StatCard label="IN GAME"         value={playerList.filter(([,p]) => p.gameStatus === 2).length} color="warning" />
          <StatCard label="IN ROOM"         value={playerList.filter(([,p]) => p.gameStatus === 1).length} color="primary" />
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Active Games */}
          <section>
            <h2 className="text-xs tracking-widest text-game-textDim mb-3">ACTIVE GAMES</h2>
            {gameList.length === 0
              ? <div className="text-game-muted text-xs py-8 text-center border border-game-border rounded">NO ACTIVE GAMES</div>
              : gameList.map(([gid, g]) => (
                <div key={gid} className="bg-game-bgCard border border-game-border rounded p-4 mb-3 hover:border-game-primary transition-colors">
                  <div className="flex justify-between items-start">
                    <div>
                      <div className="font-mono text-game-text text-sm">{g.name || `GAME #${gid}`}</div>
                      <div className="text-xs text-game-textDim mt-0.5">{g.mapName}</div>
                    </div>
                    <span className={`text-xs font-mono ${STATE_COLORS[g.state] || 'text-game-muted'}`}>
                      {STATE_LABELS[g.state] || g.state?.toUpperCase()}
                    </span>
                  </div>
                  {g.hostname && (
                    <div className="text-xs text-game-muted mt-2">{g.hostname}:{g.port}</div>
                  )}
                </div>
              ))
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
