'use client';
import Link from 'next/link';
import { usePlayerAuth, playerLogout } from '../../lib/auth';
import { getGameStatsRecent, getGameStatsLeaderboard, getGameDetail, getAgentDetail } from '../../lib/api';
import { useState, useEffect, useCallback, useMemo } from 'react';

const AGENCY_NAMES  = ['NOXIS', 'LAZARUS', 'CALIBER', 'STATIC', 'BLACKROSE'];

interface AgencyColor { text: string; bg: string; border: string; }
const AGENCY_COLORS: Record<string, AgencyColor> = {
  NOXIS:     { text: 'text-game-primary',  bg: 'bg-green-900/20',  border: 'border-green-700' },
  LAZARUS:   { text: 'text-cyan-400',      bg: 'bg-cyan-900/20',   border: 'border-cyan-700' },
  CALIBER:   { text: 'text-yellow-400',    bg: 'bg-yellow-900/20', border: 'border-yellow-700' },
  STATIC:    { text: 'text-purple-400',    bg: 'bg-purple-900/20', border: 'border-purple-700' },
  BLACKROSE: { text: 'text-rose-400',      bg: 'bg-rose-900/20',   border: 'border-rose-700' },
};
const RANK_STYLE = ['', 'text-yellow-300', 'text-slate-300', 'text-amber-600'];

function fmt(n?: number | null): string  { return (n ?? 0).toLocaleString(); }
function kd(k?: number, d?: number): string | number { return d ? (k! / d).toFixed(2) : (k || 0); }
function fmtDate(d?: string): string { return d ? new Date(d).toLocaleDateString(undefined, { month:'short', day:'numeric', year:'numeric' }) : '—'; }

interface AgencyTagProps { name: string; small?: boolean; }
function AgencyTag({ name }: AgencyTagProps) {
  const c = AGENCY_COLORS[name] ?? { text: 'text-game-text', bg: '', border: 'border-game-border' };
  return <span className={`font-mono font-bold text-xs ${c.text}`}>{name}</span>;
}

interface SortBtnProps { col: string; label: string; sortKey: string; sortDir: string; onSort: (col: string) => void; }
function SortBtn({ col, label, sortKey, sortDir, onSort }: SortBtnProps) {
  const active = sortKey === col;
  return (
    <button onClick={() => onSort(col)}
      className={`px-2 py-1 text-xs font-mono rounded border transition-colors
        ${active ? 'border-game-primary text-game-primary bg-game-dark' : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-primary'}`}>
      {label}{active ? (sortDir === 'desc' ? ' ▼' : ' ▲') : ''}
    </button>
  );
}

interface GameParticipant {
  name: string; agencyName: string; won: boolean;
  kills: number; deaths: number; xp: number;
  secretsReturned: number; filesHacked: number;
}
interface GameDetailData { participants?: GameParticipant[]; error?: boolean; }
interface GameDetailCache { [gameId: string]: GameDetailData; }

function GameDetail({ gameId, cache, setCache }: {
  gameId: string;
  cache: GameDetailCache;
  setCache: React.Dispatch<React.SetStateAction<GameDetailCache>>;
}) {
  const [loading, setLoading] = useState(!cache[gameId]);

  useEffect(() => {
    if (cache[gameId]) return;
    (getGameDetail(gameId) as Promise<GameDetailData>)
      .then(d => setCache(c => ({ ...c, [gameId]: d })))
      .catch(() => setCache(c => ({ ...c, [gameId]: { error: true } })))
      .finally(() => setLoading(false));
  }, [gameId]); // eslint-disable-line react-hooks/exhaustive-deps

  const d = cache[gameId];
  if (loading) return <div className="px-6 py-3 text-game-muted text-xs font-mono animate-pulse">LOADING...</div>;
  if (!d || d.error) return <div className="px-6 py-3 text-game-danger text-xs font-mono">FAILED TO LOAD</div>;
  if (!d.participants?.length) return <div className="px-6 py-3 text-game-muted text-xs font-mono">NO PARTICIPANTS RECORDED</div>;

  return (
    <div className="border-t border-game-border bg-game-bg/40">
      <table className="w-full text-xs font-mono">
        <thead>
          <tr className="text-game-muted border-b border-game-border/50">
            <th className="text-left px-6 py-2">AGENT</th>
            <th className="text-left px-3 py-2">AGENCY</th>
            <th className="text-center px-3 py-2">RESULT</th>
            <th className="text-right px-3 py-2">K</th>
            <th className="text-right px-3 py-2">D</th>
            <th className="text-right px-3 py-2">K/D</th>
            <th className="text-right px-3 py-2">XP</th>
            <th className="text-right px-3 py-2">SECRETS</th>
            <th className="text-right px-6 py-2">HACKED</th>
          </tr>
        </thead>
        <tbody>
          {d.participants.map((p, i) => (
            <tr key={i} className="border-b border-game-border/30 last:border-0">
              <td className="px-6 py-2 text-game-text font-semibold">{p.name}</td>
              <td className="px-3 py-2"><AgencyTag name={p.agencyName} /></td>
              <td className="px-3 py-2 text-center">
                <span className={`px-2 py-0.5 rounded text-xs font-bold ${p.won ? 'text-game-primary bg-green-900/30' : 'text-game-danger bg-red-900/20'}`}>
                  {p.won ? 'WIN' : 'LOSS'}
                </span>
              </td>
              <td className="px-3 py-2 text-right text-game-text">{p.kills}</td>
              <td className="px-3 py-2 text-right text-game-muted">{p.deaths}</td>
              <td className="px-3 py-2 text-right text-game-textDim">{kd(p.kills, p.deaths)}</td>
              <td className="px-3 py-2 text-right text-cyan-400">{p.xp}</td>
              <td className="px-3 py-2 text-right text-game-text">{p.secretsReturned}</td>
              <td className="px-6 py-2 text-right text-game-text">{p.filesHacked}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

interface AgencyEntry { agencyIdx: number; agencyName: string; level: number; wins: number; losses: number; }
interface AgentDetailData {
  agencies: AgencyEntry[];
  kills: number; deaths: number;
  totalXP: number; totalGames: number;
  firstSeen?: string;
  error?: boolean;
}
interface AgentDetailCache { [accountId: string]: AgentDetailData; }

function AgentDetail({ accountId, cache, setCache }: {
  accountId: string;
  cache: AgentDetailCache;
  setCache: React.Dispatch<React.SetStateAction<AgentDetailCache>>;
}) {
  const [loading, setLoading] = useState(!cache[accountId]);

  useEffect(() => {
    if (cache[accountId]) return;
    (getAgentDetail(accountId) as Promise<AgentDetailData>)
      .then(d => setCache(c => ({ ...c, [accountId]: d })))
      .catch(() => setCache(c => ({ ...c, [accountId]: { agencies: [], kills: 0, deaths: 0, totalXP: 0, totalGames: 0, error: true } })))
      .finally(() => setLoading(false));
  }, [accountId]); // eslint-disable-line react-hooks/exhaustive-deps

  const d = cache[accountId];
  if (loading) return <div className="px-6 py-3 text-game-muted text-xs font-mono animate-pulse">LOADING...</div>;
  if (!d || d.error) return <div className="px-6 py-3 text-game-danger text-xs font-mono">FAILED TO LOAD</div>;

  return (
    <div className="border-t border-game-border bg-game-bg/40 px-6 py-4">
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <div>
          <div className="text-game-muted text-xs font-mono mb-2 tracking-widest">ALL AGENCIES</div>
          <div className="space-y-1">
            {d.agencies.map(a => {
              const c = AGENCY_COLORS[a.agencyName] ?? { text: 'text-game-text', border: 'border-game-border' };
              const hasActivity = a.wins > 0 || a.losses > 0;
              return (
                <div key={a.agencyIdx}
                  className={`flex items-center gap-3 px-3 py-1.5 rounded border text-xs font-mono
                    ${hasActivity ? `${c.border} bg-game-bgCard` : 'border-game-border/30 opacity-40'}`}>
                  <span className={`font-bold w-20 ${c.text}`}>{a.agencyName}</span>
                  <span className="text-game-muted">Lvl {a.level}</span>
                  <span className="text-game-primary ml-auto">W:{a.wins}</span>
                  <span className="text-game-muted">L:{a.losses}</span>
                </div>
              );
            })}
          </div>
        </div>
        <div>
          <div className="text-game-muted text-xs font-mono mb-2 tracking-widest">LIFETIME COMBAT</div>
          <div className="space-y-1 text-xs font-mono">
            {([
              ['KILLS',    fmt(d.kills),              'text-game-text'],
              ['DEATHS',   fmt(d.deaths),              'text-game-muted'],
              ['K/D',      kd(d.kills, d.deaths),      'text-game-primary'],
              ['TOTAL XP', fmt(d.totalXP),             'text-cyan-400'],
              ['GAMES',    fmt(d.totalGames),           'text-game-textDim'],
              ['MEMBER SINCE', fmtDate(d.firstSeen),   'text-game-muted'],
            ] as [string, string | number, string][]).map(([label, value, cls]) => (
              <div key={label} className="flex justify-between items-center py-1 border-b border-game-border/30">
                <span className="text-game-textDim">{label}</span>
                <span className={`font-bold ${cls}`}>{value}</span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}

interface GameEntry {
  gameId: string; mapName: string; creatorName: string;
  endedAt?: string; winners: string[];
}
interface AgentEntry {
  accountId: string; name: string; rank: number; agencyName: string;
  wins: number; losses: number; level: number;
  kills: number; deaths: number; totalXP: number;
}

export default function GameStatsPage() {
  usePlayerAuth();

  const [games,       setGames]       = useState<GameEntry[] | null>(null);
  const [agents,      setAgents]      = useState<AgentEntry[] | null>(null);
  const [error,       setError]       = useState<string | null>(null);
  const [loading,     setLoading]     = useState(true);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);

  const [expandedGame,  setExpandedGame]  = useState<string | null>(null);
  const [expandedAgent, setExpandedAgent] = useState<string | null>(null);
  const [gameCache,     setGameCache]     = useState<GameDetailCache>({});
  const [agentCache,    setAgentCache]    = useState<AgentDetailCache>({});

  const [search,       setSearch]       = useState('');
  const [sortKey,      setSortKey]      = useState('totalWins');
  const [sortDir,      setSortDir]      = useState<'asc' | 'desc'>('desc');
  const [agencyFilter, setAgencyFilter] = useState('');

  const playerName = typeof window !== 'undefined'
    ? (JSON.parse(localStorage.getItem('zs_player') || '{}') as { name?: string }).name ?? '' : '';

  const load = useCallback(() => {
    setLoading(true);
    setError(null);
    Promise.all([
      getGameStatsRecent(20) as Promise<{ games: GameEntry[] }>,
      getGameStatsLeaderboard(100) as Promise<{ agents: AgentEntry[] }>,
    ])
      .then(([r, l]) => {
        setGames(r.games);
        setAgents(l.agents);
        setLastUpdated(new Date());
      })
      .catch(e => setError((e as Error).message))
      .finally(() => setLoading(false));
  }, []);

  useEffect(() => { load(); }, [load]);

  const handleSort = (col: string) => {
    if (sortKey === col) setSortDir(d => d === 'desc' ? 'asc' : 'desc');
    else { setSortKey(col); setSortDir('desc'); }
    setExpandedAgent(null);
  };

  const toggleGame  = (id: string) => setExpandedGame(g  => g  === id ? null : id);
  const toggleAgent = (id: string) => setExpandedAgent(a => a  === id ? null : id);

  const sortedAgents = useMemo(() => {
    if (!agents) return [];
    let list = agents;
    if (search)       list = list.filter(a => a.name.toLowerCase().includes(search.toLowerCase()));
    if (agencyFilter) list = list.filter(a => a.agencyName === agencyFilter);

    const dir = sortDir === 'desc' ? -1 : 1;
    const keyMap: Record<string, (a: AgentEntry) => number> = {
      totalWins: a => a.wins,
      kills:     a => a.kills,
      deaths:    a => a.deaths,
      totalXP:   a => a.totalXP,
      level:     a => a.level,
      kd:        a => a.deaths ? a.kills / a.deaths : a.kills,
    };
    const fn = keyMap[sortKey] ?? ((a: AgentEntry) => a.wins);
    return [...list].sort((a, b) => dir * (fn(b) - fn(a)));
  }, [agents, search, agencyFilter, sortKey, sortDir]);

  return (
    <div className="min-h-screen">
      <header className="flex items-center justify-between px-6 py-4 border-b border-game-border bg-game-bgCard">
        <div>
          <img src="/logo.png" alt="Silencer" className="h-10 w-auto" />
          <div className="text-game-textDim text-xs font-mono tracking-widest">PLAYER PORTAL</div>
        </div>
        <div className="flex items-center gap-3">
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
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-game-primary font-mono text-xl tracking-widest">◉ COMMUNITY STATS</h1>
          <div className="flex items-center gap-3">
            {lastUpdated && (
              <span className="text-game-muted text-xs font-mono">Updated {lastUpdated.toLocaleTimeString()}</span>
            )}
            <button onClick={load} disabled={loading}
              className="px-3 py-1.5 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-primary transition-colors disabled:opacity-40">
              {loading ? '⟳ LOADING...' : '↻ REFRESH'}
            </button>
          </div>
        </div>

        {error && (
          <div className="bg-red-900/20 border border-game-danger text-game-danger text-xs font-mono px-4 py-2 rounded mb-4">
            {error}
          </div>
        )}

        {/* Recent Games */}
        <div className="bg-game-bgCard border border-game-border rounded mb-6 overflow-hidden">
          <div className="px-4 py-3 border-b border-game-border">
            <h2 className="text-game-primary font-mono text-sm tracking-widest">◑ RECENT GAMES</h2>
          </div>
          {!games
            ? <div className="px-4 py-6 text-game-muted text-xs font-mono animate-pulse">LOADING...</div>
            : games.length === 0
              ? <div className="px-4 py-6 text-game-muted text-xs font-mono">NO COMPLETED GAMES YET</div>
              : (
                <div>
                  <div className="grid grid-cols-[2rem_4rem_1fr_1fr_7rem_1fr] gap-2 px-4 py-2 border-b border-game-border text-game-muted text-xs font-mono">
                    <div /><div>ID</div><div>MAP</div><div>CREATOR</div><div>DATE</div><div>WINNERS</div>
                  </div>
                  {games.map(g => (
                    <div key={g.gameId}>
                      <button
                        onClick={() => toggleGame(g.gameId)}
                        className="w-full grid grid-cols-[2rem_4rem_1fr_1fr_7rem_1fr] gap-2 px-4 py-2.5 items-center border-b border-game-border/50 hover:bg-game-bgHover transition-colors text-left text-xs font-mono">
                        <span className="text-game-muted text-center">{expandedGame === g.gameId ? '▼' : '▶'}</span>
                        <span className="text-game-muted">#{g.gameId}</span>
                        <span className="text-game-text font-semibold">{g.mapName}</span>
                        <span className="text-game-primary">{g.creatorName}</span>
                        <span className="text-game-textDim">{fmtDate(g.endedAt)}</span>
                        <span>
                          {g.winners.length === 0
                            ? <span className="text-game-muted">—</span>
                            : <>
                                <span className="text-game-primary">{g.winners.slice(0, 4).join(', ')}</span>
                                {g.winners.length > 4 && <span className="text-game-muted ml-1">+{g.winners.length - 4}</span>}
                              </>
                          }
                        </span>
                      </button>
                      {expandedGame === g.gameId && (
                        <GameDetail gameId={g.gameId} cache={gameCache} setCache={setGameCache} />
                      )}
                    </div>
                  ))}
                </div>
              )
          }
        </div>

        {/* Top Agents */}
        <div className="bg-game-bgCard border border-game-border rounded overflow-hidden">
          <div className="px-4 py-3 border-b border-game-border">
            <h2 className="text-game-primary font-mono text-sm tracking-widest mb-3">◈ TOP AGENTS</h2>
            <div className="flex flex-wrap gap-2 mb-3">
              <button onClick={() => { setAgencyFilter(''); setExpandedAgent(null); }}
                className={`px-2 py-1 text-xs font-mono rounded border transition-colors
                  ${!agencyFilter ? 'border-game-primary text-game-primary bg-game-dark' : 'border-game-border text-game-textDim hover:border-game-primary'}`}>
                ALL
              </button>
              {AGENCY_NAMES.map(n => {
                const c = AGENCY_COLORS[n];
                const active = agencyFilter === n;
                return (
                  <button key={n} onClick={() => { setAgencyFilter(active ? '' : n); setExpandedAgent(null); }}
                    className={`px-2 py-1 text-xs font-mono rounded border transition-colors
                      ${active ? `${c.border} ${c.text} ${c.bg}` : 'border-game-border text-game-textDim hover:border-game-primary'}`}>
                    {n}
                  </button>
                );
              })}
            </div>
            <div className="flex flex-wrap gap-2 items-center">
              <input
                value={search}
                onChange={e => { setSearch(e.target.value); setExpandedAgent(null); }}
                placeholder="SEARCH AGENT..."
                className="bg-game-bg border border-game-border text-game-text font-mono text-xs px-3 py-1.5 rounded w-48 focus:outline-none focus:border-game-primary placeholder-game-muted"
              />
              <div className="flex gap-1">
                {([
                  ['totalWins', 'WINS'], ['level', 'LEVEL'], ['kills', 'KILLS'],
                  ['kd', 'K/D'], ['totalXP', 'XP'],
                ] as [string, string][]).map(([col, label]) => (
                  <SortBtn key={col} col={col} label={label} sortKey={sortKey} sortDir={sortDir} onSort={handleSort} />
                ))}
              </div>
              {(search || agencyFilter) && (
                <span className="text-game-muted text-xs font-mono">{sortedAgents.length} results</span>
              )}
            </div>
          </div>

          {!agents
            ? <div className="px-4 py-6 text-game-muted text-xs font-mono animate-pulse">LOADING...</div>
            : sortedAgents.length === 0
              ? <div className="px-4 py-6 text-game-muted text-xs font-mono">NO AGENTS MATCH</div>
              : (
                <div>
                  <div className="grid grid-cols-[2rem_2rem_1fr_5rem_3rem_3rem_3rem_4rem_4rem_5rem] gap-2 px-4 py-2 border-b border-game-border text-game-muted text-xs font-mono">
                    <div /><div>#</div><div>NAME</div><div>AGENCY</div>
                    <div className="text-right">W</div><div className="text-right">L</div>
                    <div className="text-right">LVL</div><div className="text-right">KILLS</div>
                    <div className="text-right">K/D</div><div className="text-right">TOTAL XP</div>
                  </div>
                  {sortedAgents.map((a) => {
                    const rankCls = RANK_STYLE[a.rank] || 'text-game-muted';
                    const isMine  = a.name === playerName;
                    const agColor = AGENCY_COLORS[a.agencyName]?.text ?? 'text-game-text';
                    return (
                      <div key={a.rank}>
                        <button
                          onClick={() => toggleAgent(a.accountId)}
                          className={`w-full grid grid-cols-[2rem_2rem_1fr_5rem_3rem_3rem_3rem_4rem_4rem_5rem] gap-2 px-4 py-2.5 items-center border-b border-game-border/50 hover:bg-game-bgHover transition-colors text-left text-xs font-mono
                            ${isMine ? 'bg-game-dark/50' : ''}`}>
                          <span className="text-game-muted text-center">{expandedAgent === a.accountId ? '▼' : '▶'}</span>
                          <span className={`font-bold ${rankCls}`}>{a.rank}</span>
                          <span className={`font-semibold ${isMine ? 'text-game-primary' : 'text-game-text'}`}>
                            {a.name}{isMine && <span className="text-game-muted text-xs ml-1">(you)</span>}
                          </span>
                          <span className={`font-bold ${agColor}`}>{a.agencyName}</span>
                          <span className="text-right text-game-primary font-bold">{fmt(a.wins)}</span>
                          <span className="text-right text-game-muted">{fmt(a.losses)}</span>
                          <span className="text-right text-game-text">{a.level}</span>
                          <span className="text-right text-game-text">{fmt(a.kills)}</span>
                          <span className="text-right text-game-textDim">{kd(a.kills, a.deaths)}</span>
                          <span className="text-right text-cyan-400">{fmt(a.totalXP)}</span>
                        </button>
                        {expandedAgent === a.accountId && (
                          <AgentDetail accountId={a.accountId} cache={agentCache} setCache={setAgentCache} />
                        )}
                      </div>
                    );
                  })}
                </div>
              )
          }
        </div>
      </main>
    </div>
  );
}
