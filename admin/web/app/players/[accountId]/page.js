'use client';
import { useAuth } from '../../../lib/auth.js';
import { useSocket } from '../../../lib/socket.js';
import Sidebar from '../../../components/Sidebar.js';
import { useState, useEffect, useCallback } from 'react';
import { useParams, useRouter } from 'next/navigation';
import { getPlayer, getPlayerMatches, banPlayer, deletePlayer } from '../../../lib/api.js';

const AGENCY_NAMES = ['NOXIS', 'LAZARUS', 'CALIBER', 'STATIC', 'BLACKROSE'];

const WEAPONS = [
  { key: 'blaster', label: 'BLASTER' },
  { key: 'laser',   label: 'LASER'   },
  { key: 'rocket',  label: 'ROCKET'  },
  { key: 'flamer',  label: 'FLAMER'  },
];

function pct(hits, fires) {
  if (!fires) return '—';
  return `${Math.round((hits / fires) * 100)}%`;
}

function StatRow({ label, value }) {
  return (
    <div className="flex justify-between text-xs font-mono py-1 border-b border-game-border/30">
      <span className="text-game-muted">{label}</span>
      <span className="text-game-text">{value ?? 0}</span>
    </div>
  );
}

export default function PlayerDetail() {
  useAuth();
  const wsConnected = useSocket({});
  const router      = useRouter();
  const { accountId } = useParams();

  const [player, setPlayer]   = useState(null);
  const [matches, setMatches] = useState([]);
  const [total, setTotal]     = useState(0);
  const [page, setPage]       = useState(1);
  const [error, setError]     = useState(null);

  const load = useCallback(async () => {
    try {
      const [p, m] = await Promise.all([
        getPlayer(accountId),
        getPlayerMatches(accountId, page),
      ]);
      setPlayer(p);
      setMatches(m.matches);
      setTotal(m.total);
    } catch (e) {
      setError(e.message);
    }
  }, [accountId, page]);

  useEffect(() => { load(); }, [load]);

  const handleBan = async () => {
    await banPlayer(accountId, !player.banned, !player.banned ? 'Admin action' : '');
    load();
  };

  const handleDelete = async () => {
    if (!confirm(`Permanently delete ${player.name}? This cannot be undone.`)) return;
    await deletePlayer(accountId);
    router.push('/players');
  };

  if (error) return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 flex items-center justify-center">
        <div className="text-game-danger font-mono text-sm">{error}</div>
      </main>
    </div>
  );

  if (!player) return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 flex items-center justify-center">
        <div className="text-game-muted font-mono text-xs animate-pulse">LOADING...</div>
      </main>
    </div>
  );

  const ls = player.lifetimeStats || {};
  const totalPages = Math.ceil(total / 20);

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">

        {/* Header */}
        <div className="flex items-center gap-4 mb-6">
          <button onClick={() => router.push('/players')}
            className="text-game-muted hover:text-game-primary font-mono text-xs transition-colors">
            ← PLAYERS
          </button>
          <h1 className="text-game-primary font-mono text-xl tracking-widest">◈ {player.name}</h1>
          <span className={`text-xs font-mono px-2 py-0.5 rounded border ${player.banned ? 'border-game-danger text-game-danger' : 'border-game-primary text-game-primary'}`}>
            {player.banned ? 'BANNED' : 'ACTIVE'}
          </span>
          <button onClick={handleBan}
            className={`ml-auto px-3 py-1 text-xs font-mono border rounded transition-colors
              ${player.banned
                ? 'border-game-primary text-game-primary hover:bg-game-dark'
                : 'border-game-danger text-game-danger hover:bg-red-900/20'}`}>
            {player.banned ? 'UNBAN' : 'BAN PLAYER'}
          </button>
          <button onClick={handleDelete}
            className="px-3 py-1 text-xs font-mono border border-red-700 text-red-500 rounded hover:bg-red-900/30 transition-colors">
            🗑 DELETE
          </button>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-8">

          {/* Identity */}
          <div className="bg-game-bgCard border border-game-border rounded p-4">
            <h2 className="text-xs text-game-textDim tracking-widest mb-3">IDENTITY</h2>
            <StatRow label="ACCOUNT ID"  value={player.accountId} />
            <StatRow label="LOGINS"      value={player.loginCount} />
            <StatRow label="FIRST SEEN"  value={player.firstSeen ? new Date(player.firstSeen).toLocaleDateString() : '—'} />
            <StatRow label="LAST SEEN"   value={player.lastSeen  ? new Date(player.lastSeen).toLocaleDateString()  : '—'} />
            {player.banReason && <StatRow label="BAN REASON" value={player.banReason} />}
          </div>

          {/* Combat */}
          <div className="bg-game-bgCard border border-game-border rounded p-4">
            <h2 className="text-xs text-game-textDim tracking-widest mb-3">LIFETIME COMBAT</h2>
            <StatRow label="KILLS"       value={ls.kills} />
            <StatRow label="DEATHS"      value={ls.deaths} />
            <StatRow label="K/D"         value={ls.deaths ? (ls.kills / ls.deaths).toFixed(2) : ls.kills || 0} />
            <StatRow label="SUICIDES"    value={ls.suicides} />
            <StatRow label="POISONS"     value={ls.poisons} />
            <StatRow label="CIVILIANS ✝" value={ls.civiliansKilled} />
            <StatRow label="GUARDS ✝"    value={ls.guardsKilled} />
            <StatRow label="ROBOTS ✝"    value={ls.robotsKilled} />
          </div>

          {/* Economy */}
          <div className="bg-game-bgCard border border-game-border rounded p-4">
            <h2 className="text-xs text-game-textDim tracking-widest mb-3">ECONOMY</h2>
            <StatRow label="CREDITS EARNED" value={ls.creditsmade} />
            <StatRow label="CREDITS SPENT"  value={ls.creditsspent} />
            <StatRow label="HEALS DONE"     value={ls.healsdone} />
            <StatRow label="HEALTH PACKS"   value={ls.healthPacksUsed} />
            <StatRow label="POWERUPS"       value={ls.powerupsPickedUp} />
          </div>
        </div>

        {/* Agencies */}
        <h2 className="text-xs text-game-textDim tracking-widest mb-3">AGENCIES</h2>
        <div className="grid grid-cols-2 lg:grid-cols-5 gap-3 mb-8">
          {(player.agencies || []).map((a, i) => (
            <div key={i} className="bg-game-bgCard border border-game-border rounded p-3 text-xs font-mono">
              <div className="text-game-primary font-semibold mb-2">{AGENCY_NAMES[i] || `AGY ${i+1}`}</div>
              <StatRow label="LEVEL"  value={a.level} />
              <StatRow label="W / L"  value={`${a.wins} / ${a.losses}`} />
              <StatRow label="XP TO NEXT" value={a.xpToNextLevel} />
            </div>
          ))}
        </div>

        {/* Weapon Accuracy */}
        <h2 className="text-xs text-game-textDim tracking-widest mb-3">WEAPON ACCURACY</h2>
        <div className="bg-game-bgCard border border-game-border rounded overflow-hidden mb-8">
          <table className="w-full text-xs font-mono">
            <thead>
              <tr className="border-b border-game-border text-game-textDim">
                <th className="text-left px-4 py-2">WEAPON</th>
                <th className="text-right px-4 py-2">FIRES</th>
                <th className="text-right px-4 py-2">HITS</th>
                <th className="text-right px-4 py-2">KILLS</th>
                <th className="text-right px-4 py-2">ACC</th>
              </tr>
            </thead>
            <tbody>
              {WEAPONS.map(w => (
                <tr key={w.key} className="border-b border-game-border/30 text-game-text">
                  <td className="px-4 py-2 text-game-muted">{w.label}</td>
                  <td className="px-4 py-2 text-right">{ls[`${w.key}Fires`] ?? 0}</td>
                  <td className="px-4 py-2 text-right">{ls[`${w.key}Hits`]  ?? 0}</td>
                  <td className="px-4 py-2 text-right">{ls[`${w.key}Kills`] ?? 0}</td>
                  <td className="px-4 py-2 text-right text-game-primary">{pct(ls[`${w.key}Hits`], ls[`${w.key}Fires`])}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {/* Match History */}
        <h2 className="text-xs text-game-textDim tracking-widest mb-3">MATCH HISTORY ({total})</h2>
        <div className="bg-game-bgCard border border-game-border rounded overflow-hidden mb-4">
          <table className="w-full text-xs font-mono">
            <thead>
              <tr className="border-b border-game-border text-game-textDim">
                <th className="text-left px-4 py-2">DATE</th>
                <th className="text-left px-4 py-2">AGENCY</th>
                <th className="text-right px-4 py-2">K</th>
                <th className="text-right px-4 py-2">D</th>
                <th className="text-right px-4 py-2">RESULT</th>
              </tr>
            </thead>
            <tbody>
              {matches.length === 0 && (
                <tr><td colSpan={5} className="px-4 py-4 text-center text-game-muted">No matches recorded.</td></tr>
              )}
              {matches.map(m => (
                <tr key={m._id} className="border-b border-game-border/30">
                  <td className="px-4 py-2 text-game-muted">{new Date(m.createdAt).toLocaleString()}</td>
                  <td className="px-4 py-2 text-game-text">{AGENCY_NAMES[m.team] ?? m.team}</td>
                  <td className="px-4 py-2 text-right text-game-text">{m.kills ?? 0}</td>
                  <td className="px-4 py-2 text-right text-game-text">{m.deaths ?? 0}</td>
                  <td className={`px-4 py-2 text-right ${m.win ? 'text-game-primary' : 'text-game-danger'}`}>
                    {m.win ? 'WIN' : 'LOSS'}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {totalPages > 1 && (
          <div className="flex gap-2 text-xs font-mono">
            {Array.from({ length: totalPages }, (_, i) => i + 1).slice(Math.max(0, page - 3), page + 2).map(n => (
              <button key={n} onClick={() => setPage(n)}
                className={`px-3 py-1 border rounded ${n === page ? 'border-game-primary text-game-primary' : 'border-game-border text-game-textDim hover:border-game-primary'}`}>
                {n}
              </button>
            ))}
          </div>
        )}

      </main>
    </div>
  );
}
