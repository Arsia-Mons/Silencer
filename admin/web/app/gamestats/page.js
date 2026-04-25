'use client';
import Link from 'next/link';
import { usePlayerAuth, playerLogout } from '../../lib/auth.js';
import { getGameStatsRecent, getGameStatsLeaderboard } from '../../lib/api.js';
import { useState, useEffect } from 'react';

const AGENCY_COLORS = {
  NOXIS:     'text-game-primary',
  LAZARUS:   'text-cyan-400',
  CALIBER:   'text-yellow-400',
  STATIC:    'text-purple-400',
  BLACKROSE: 'text-rose-400',
};

function Section({ title, icon, children }) {
  return (
    <div className="bg-game-bgCard border border-game-border rounded p-4 mb-6">
      <h2 className="text-game-primary font-mono text-sm tracking-widest mb-3">{icon} {title}</h2>
      {children}
    </div>
  );
}

function fmt(n) { return (n ?? 0).toLocaleString(); }

export default function GameStatsPage() {
  usePlayerAuth();
  const [games, setGames]   = useState(null);
  const [agents, setAgents] = useState(null);
  const [error, setError]   = useState(null);

  const playerName = typeof window !== 'undefined'
    ? JSON.parse(localStorage.getItem('zs_player') || '{}').name : '';

  useEffect(() => {
    Promise.all([
      getGameStatsRecent(20),
      getGameStatsLeaderboard(50),
    ]).then(([r, l]) => {
      setGames(r.games);
      setAgents(l.agents);
    }).catch(e => setError(e.message));
  }, []);

  return (
    <div className="min-h-screen">
      {/* Header */}
      <header className="flex items-center justify-between px-6 py-4 border-b border-game-border bg-game-bgCard">
        <div>
          <img src="/logo.png" alt="zSILENCER" className="h-10 w-auto" />
          <div className="text-game-textDim text-xs font-mono tracking-widest">PLAYER PORTAL</div>
        </div>
        <div className="flex items-center gap-4">
          <span className="text-game-text font-mono text-sm">◈ {playerName}</span>
          <Link href="/me"
            className="px-3 py-1.5 text-xs font-mono text-game-textDim border border-game-border rounded hover:border-game-primary hover:text-game-primary transition-colors">
            [ MY STATS ]
          </Link>
          <Link href="/howto"
            className="px-3 py-1.5 text-xs font-mono text-game-textDim border border-game-border rounded hover:border-game-primary hover:text-game-primary transition-colors">
            [ HOW TO PLAY ]
          </Link>
          <button onClick={playerLogout}
            className="px-3 py-1.5 text-xs font-mono text-game-muted border border-game-border rounded hover:border-game-danger hover:text-game-danger transition-colors">
            [ LOGOUT ]
          </button>
        </div>
      </header>

      <main className="max-w-6xl mx-auto p-6">
        <h1 className="text-game-primary font-mono text-xl tracking-widest mb-6">◉ COMMUNITY STATS</h1>

        {error && (
          <div className="bg-red-900/20 border border-game-danger text-game-danger text-xs font-mono px-4 py-2 rounded mb-4">
            {error}
          </div>
        )}

        {/* Recent Games */}
        <Section title="RECENT GAMES" icon="◑">
          {!games
            ? <p className="text-game-muted text-xs font-mono animate-pulse">LOADING...</p>
            : games.length === 0
              ? <p className="text-game-muted text-xs font-mono">NO COMPLETED GAMES YET</p>
              : (
                <div className="overflow-x-auto">
                  <table className="w-full text-xs font-mono">
                    <thead>
                      <tr className="border-b border-game-border text-game-textDim">
                        <th className="text-left px-3 py-2">ID</th>
                        <th className="text-left px-3 py-2">MAP</th>
                        <th className="text-left px-3 py-2">CREATOR</th>
                        <th className="text-left px-3 py-2">DATE</th>
                        <th className="text-left px-3 py-2">WINNERS</th>
                      </tr>
                    </thead>
                    <tbody>
                      {games.map(g => (
                        <tr key={g.gameId} className="border-b border-game-border last:border-0 hover:bg-game-bgHover">
                          <td className="px-3 py-2 text-game-muted">#{g.gameId}</td>
                          <td className="px-3 py-2 text-game-text">{g.mapName}</td>
                          <td className="px-3 py-2 text-game-primary">{g.creatorName}</td>
                          <td className="px-3 py-2 text-game-textDim">
                            {g.endedAt ? new Date(g.endedAt).toLocaleDateString() : '—'}
                          </td>
                          <td className="px-3 py-2">
                            {g.winners.length === 0
                              ? <span className="text-game-muted">—</span>
                              : <>
                                  <span className="text-game-primary">
                                    {g.winners.slice(0, 5).join(', ')}
                                  </span>
                                  {g.winners.length > 5 && (
                                    <span className="text-game-muted ml-1">+{g.winners.length - 5} more</span>
                                  )}
                                </>
                            }
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )
          }
        </Section>

        {/* Top Agents */}
        <Section title="TOP AGENTS" icon="◈">
          {!agents
            ? <p className="text-game-muted text-xs font-mono animate-pulse">LOADING...</p>
            : agents.length === 0
              ? <p className="text-game-muted text-xs font-mono">NO RANKED AGENTS YET</p>
              : (
                <div className="overflow-x-auto">
                  <table className="w-full text-xs font-mono">
                    <thead>
                      <tr className="border-b border-game-border text-game-textDim">
                        <th className="text-left px-3 py-2">#</th>
                        <th className="text-left px-3 py-2">NAME</th>
                        <th className="text-left px-3 py-2">AGENCY</th>
                        <th className="text-right px-3 py-2">WINS</th>
                        <th className="text-right px-3 py-2">LOSSES</th>
                        <th className="text-right px-3 py-2">LEVEL</th>
                        <th className="text-right px-3 py-2">KILLS</th>
                        <th className="text-right px-3 py-2">DEATHS</th>
                        <th className="text-right px-3 py-2">TOTAL XP</th>
                      </tr>
                    </thead>
                    <tbody>
                      {agents.map(a => (
                        <tr key={a.rank}
                          className={`border-b border-game-border last:border-0 hover:bg-game-bgHover
                            ${a.name === playerName ? 'bg-game-dark/60' : ''}`}>
                          <td className="px-3 py-2 text-game-muted">
                            {a.rank <= 3
                              ? <span className="text-yellow-400 font-bold">{a.rank}</span>
                              : a.rank}
                          </td>
                          <td className="px-3 py-2 text-game-text font-semibold">
                            {a.name}
                            {a.name === playerName && <span className="text-game-muted ml-1">(you)</span>}
                          </td>
                          <td className={`px-3 py-2 font-semibold ${AGENCY_COLORS[a.agencyName] ?? 'text-game-text'}`}>
                            {a.agencyName}
                          </td>
                          <td className="px-3 py-2 text-right text-game-primary">{fmt(a.wins)}</td>
                          <td className="px-3 py-2 text-right text-game-muted">{fmt(a.losses)}</td>
                          <td className="px-3 py-2 text-right text-game-text">{a.level}</td>
                          <td className="px-3 py-2 text-right text-game-text">{fmt(a.kills)}</td>
                          <td className="px-3 py-2 text-right text-game-muted">{fmt(a.deaths)}</td>
                          <td className="px-3 py-2 text-right text-cyan-400">{fmt(a.totalXP)}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )
          }
        </Section>
      </main>
    </div>
  );
}
