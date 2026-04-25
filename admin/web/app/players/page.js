'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import { useState, useEffect, useCallback } from 'react';
import { useRouter } from 'next/navigation';
import { getPlayers, banPlayer } from '../../lib/api.js';

export default function Players() {
  useAuth();
  const wsConnected = useSocket({});
  const router = useRouter();
  const [data, setData]     = useState({ players: [], total: 0 });
  const [search, setSearch] = useState('');
  const [page, setPage]     = useState(1);
  const [loading, setLoading] = useState(false);

  const load = useCallback(async () => {
    setLoading(true);
    try { setData(await getPlayers({ search, page, limit: 50 })); }
    catch (e) { console.error(e); }
    finally { setLoading(false); }
  }, [search, page]);

  useEffect(() => { load(); }, [load]);

  const handleBan = async (e, accountId, banned) => {
    e.stopPropagation();
    await banPlayer(accountId, banned, banned ? 'Admin action' : '');
    load();
  };

  const totalPages = Math.ceil(data.total / 50);

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <h1 className="text-game-primary font-mono text-xl tracking-widest mb-6">◈ PLAYERS</h1>

        <div className="mb-4 flex gap-3 items-center">
          <input
            value={search}
            onChange={e => { setSearch(e.target.value); setPage(1); }}
            placeholder="SEARCH CALLSIGN..."
            className="bg-game-bgCard border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded w-64 focus:outline-none focus:border-game-primary placeholder-game-muted"
          />
          <span className="text-xs text-game-textDim">{data.total} TOTAL</span>
          {loading && <span className="text-xs text-game-warning animate-pulse">LOADING...</span>}
        </div>

        <div className="bg-game-bgCard border border-game-border rounded overflow-hidden mb-4">
          <table className="w-full text-xs font-mono">
            <thead>
              <tr className="border-b border-game-border text-game-textDim">
                <th className="text-left px-4 py-3">ID</th>
                <th className="text-left px-4 py-3">CALLSIGN</th>
                <th className="text-left px-4 py-3">LEVEL</th>
                <th className="text-left px-4 py-3">W / L</th>
                <th className="text-left px-4 py-3">LAST SEEN</th>
                <th className="text-left px-4 py-3">STATUS</th>
                <th className="text-left px-4 py-3">ACTION</th>
              </tr>
            </thead>
            <tbody>
              {data.players.map(p => (
                <tr key={p.accountId}
                  className="border-b border-game-border last:border-0 hover:bg-game-bgHover cursor-pointer"
                  onClick={() => router.push(`/players/${p.accountId}`)}>
                  <td className="px-4 py-2 text-game-muted">{p.accountId}</td>
                  <td className="px-4 py-2 text-game-primary font-semibold">{p.name}</td>
                  <td className="px-4 py-2 text-game-text">{(p.agencies || []).reduce((b, a) => a.wins > b.wins || (a.wins === b.wins && a.level > b.level) ? a : b, p.agencies?.[0] || {}).level ?? 0}</td>
                  <td className="px-4 py-2 text-game-textDim">{(p.agencies || []).reduce((t, a) => t + (a.wins || 0), 0)} / {(p.agencies || []).reduce((t, a) => t + (a.losses || 0), 0)}</td>
                  <td className="px-4 py-2 text-game-muted">{p.lastSeen ? new Date(p.lastSeen).toLocaleDateString() : '—'}</td>
                  <td className={`px-4 py-2 ${p.banned ? 'text-game-danger' : 'text-game-primary'}`}>{p.banned ? 'BANNED' : 'ACTIVE'}</td>
                  <td className="px-4 py-2">
                    <button onClick={e => handleBan(e, p.accountId, !p.banned)}
                      className={`px-2 py-1 text-xs border rounded transition-colors
                        ${p.banned
                          ? 'border-game-primary text-game-primary hover:bg-game-dark'
                          : 'border-game-danger  text-game-danger  hover:bg-red-900/20'}`}>
                      {p.banned ? 'UNBAN' : 'BAN'}
                    </button>
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
                className={`px-3 py-1 border rounded transition-colors
                  ${n === page ? 'border-game-primary text-game-primary bg-game-dark' : 'border-game-border text-game-textDim hover:border-game-primary'}`}>
                {n}
              </button>
            ))}
          </div>
        )}
      </main>
    </div>
  );
}
