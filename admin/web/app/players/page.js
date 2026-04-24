'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import { useState, useEffect, useCallback } from 'react';
import { getPlayers, banPlayer } from '../../lib/api.js';

export default function Players() {
  useAuth();
  const wsConnected = useSocket({});
  const [data, setData]     = useState({ players: [], total: 0 });
  const [search, setSearch] = useState('');
  const [page, setPage]     = useState(1);
  const [selected, setSelected] = useState(null);
  const [loading, setLoading]   = useState(false);

  const load = useCallback(async () => {
    setLoading(true);
    try { setData(await getPlayers({ search, page, limit: 50 })); }
    catch (e) { console.error(e); }
    finally { setLoading(false); }
  }, [search, page]);

  useEffect(() => { load(); }, [load]);

  const handleBan = async (accountId, banned) => {
    await banPlayer(accountId, banned, banned ? 'Admin action' : '');
    setSelected(s => s ? { ...s, banned } : s);
    load();
  };

  const totalPages = Math.ceil(data.total / 50);

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <h1 className="text-game-primary font-mono text-xl tracking-widest mb-6">◈ PLAYERS</h1>

        {/* Search */}
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

        {/* Table */}
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
                <tr key={p.accountId} className="border-b border-game-border last:border-0 hover:bg-game-bgHover cursor-pointer"
                  onClick={() => setSelected(p)}>
                  <td className="px-4 py-2 text-game-muted">{p.accountId}</td>
                  <td className="px-4 py-2 text-game-text">{p.name}</td>
                  <td className="px-4 py-2 text-game-primary">{p.agencies?.[0]?.level ?? 0}</td>
                  <td className="px-4 py-2 text-game-textDim">{p.agencies?.[0]?.wins ?? 0} / {p.agencies?.[0]?.losses ?? 0}</td>
                  <td className="px-4 py-2 text-game-muted">{p.lastSeen ? new Date(p.lastSeen).toLocaleDateString() : '—'}</td>
                  <td className={`px-4 py-2 ${p.banned ? 'text-game-danger' : 'text-game-primary'}`}>{p.banned ? 'BANNED' : 'ACTIVE'}</td>
                  <td className="px-4 py-2" onClick={e => e.stopPropagation()}>
                    <button onClick={() => handleBan(p.accountId, !p.banned)}
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

        {/* Pagination */}
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

        {/* Player Detail Modal */}
        {selected && (
          <div className="fixed inset-0 bg-black/70 flex items-center justify-center z-50"
            onClick={() => setSelected(null)}>
            <div className="bg-game-bgCard border border-game-primary rounded p-6 w-full max-w-lg"
              onClick={e => e.stopPropagation()}>
              <div className="flex justify-between items-center mb-4">
                <h2 className="text-game-primary font-mono text-lg">// {selected.name}</h2>
                <button onClick={() => setSelected(null)} className="text-game-muted hover:text-game-text">✕</button>
              </div>
              <div className="grid grid-cols-2 gap-3 text-xs font-mono mb-4">
                <div><span className="text-game-textDim">ACCOUNT ID: </span><span className="text-game-text">{selected.accountId}</span></div>
                <div><span className="text-game-textDim">STATUS: </span><span className={selected.banned ? 'text-game-danger' : 'text-game-primary'}>{selected.banned ? 'BANNED' : 'ACTIVE'}</span></div>
                <div><span className="text-game-textDim">LOGINS: </span><span className="text-game-text">{selected.loginCount}</span></div>
                <div><span className="text-game-textDim">FIRST SEEN: </span><span className="text-game-text">{selected.firstSeen ? new Date(selected.firstSeen).toLocaleDateString() : '—'}</span></div>
              </div>
              <h3 className="text-xs text-game-textDim tracking-widest mb-2">AGENCIES</h3>
              {(selected.agencies || []).map((a, i) => (
                <div key={i} className="border border-game-border rounded px-3 py-2 mb-2 grid grid-cols-4 gap-2 text-xs font-mono">
                  <div><span className="text-game-muted">AGY {i+1}</span></div>
                  <div><span className="text-game-textDim">LVL </span><span className="text-game-primary">{a.level}</span></div>
                  <div><span className="text-game-textDim">W </span><span className="text-game-text">{a.wins}</span></div>
                  <div><span className="text-game-textDim">L </span><span className="text-game-text">{a.losses}</span></div>
                </div>
              ))}
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
