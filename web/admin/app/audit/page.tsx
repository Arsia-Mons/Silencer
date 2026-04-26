'use client';
import { useAuth } from '../../lib/auth';
import { useSocket } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import { useState, useEffect } from 'react';
import { getEvents } from '../../lib/api';
import type { AuditEvent } from '../../lib/types';

const TYPE_COLORS: Record<string, string> = {
  'player.login':   'text-game-primary',
  'player.logout':  'text-game-textDim',
  'game.created':   'text-game-info',
  'game.ready':     'text-game-warning',
  'game.ended':     'text-game-muted',
};

const EVENT_TYPES = ['', 'player.login', 'player.logout', 'game.created', 'game.ready', 'game.ended'];
const PAGE_SIZE = 50;

export default function Audit() {
  useAuth();
  const [events, setEvents] = useState<AuditEvent[]>([]);
  const [total,  setTotal]  = useState(0);
  const [page,   setPage]   = useState(1);
  const [filter, setFilter] = useState({ type: '', accountId: '' });
  const [live,   setLive]   = useState(true);
  const totalPages = Math.max(1, Math.ceil(total / PAGE_SIZE));

  const load = async (p = page) => {
    try {
      const params: Record<string, unknown> = { page: p, limit: PAGE_SIZE };
      if (filter.type) params.type = filter.type;
      if (filter.accountId) params.accountId = filter.accountId;
      const res = await getEvents(params);
      setEvents(res.events.reverse());
      setTotal(res.total);
    } catch (e) { console.error(e); }
  };

  // Reset to page 1 when filters change
  useEffect(() => { setPage(1); load(1); }, [filter]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => { load(page); }, [page]); // eslint-disable-line react-hooks/exhaustive-deps

  const wsConnected = useSocket({
    'player.login':   (...args: unknown[]) => {
      const d = args[0] as AuditEvent;
      if (live && page === totalPages) setEvents(prev => [...prev, { ...d, type: 'player.login', ts: new Date().toISOString() }]);
    },
    'player.logout':  (...args: unknown[]) => {
      const d = args[0] as AuditEvent;
      if (live && page === totalPages) setEvents(prev => [...prev, { ...d, type: 'player.logout', ts: new Date().toISOString() }]);
    },
    'game.created':   (...args: unknown[]) => {
      const d = args[0] as AuditEvent;
      if (live && page === totalPages) setEvents(prev => [...prev, { ...d, type: 'game.created', ts: new Date().toISOString() }]);
    },
    'game.ready':     (...args: unknown[]) => {
      const d = args[0] as AuditEvent;
      if (live && page === totalPages) setEvents(prev => [...prev, { ...d, type: 'game.ready', ts: new Date().toISOString() }]);
    },
    'game.ended':     (...args: unknown[]) => {
      const d = args[0] as AuditEvent;
      if (live && page === totalPages) setEvents(prev => [...prev, { ...d, type: 'game.ended', ts: new Date().toISOString() }]);
    },
  });

  const btnClass = (disabled: boolean) =>
    `px-3 py-1 border font-mono text-xs rounded transition-colors ${
      disabled
        ? 'border-game-border text-game-muted cursor-not-allowed opacity-40'
        : 'border-game-border text-game-text hover:border-game-primary hover:text-game-primary cursor-pointer'
    }`;

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 flex flex-col overflow-hidden">
        <h1 className="text-game-primary font-mono text-xl tracking-widest mb-4">◧ AUDIT LOG</h1>

        {/* Filters */}
        <div className="flex flex-wrap gap-3 mb-4 items-center">
          <select value={filter.type} onChange={(e: React.ChangeEvent<HTMLSelectElement>) => setFilter(f => ({ ...f, type: e.target.value }))}
            className="bg-game-bgCard border border-game-border text-game-text font-mono text-xs px-3 py-2 rounded focus:outline-none focus:border-game-primary">
            {EVENT_TYPES.map(t => <option key={t} value={t}>{t || 'ALL TYPES'}</option>)}
          </select>
          <input value={filter.accountId} onChange={(e: React.ChangeEvent<HTMLInputElement>) => setFilter(f => ({ ...f, accountId: e.target.value }))}
            placeholder="ACCOUNT ID..."
            className="bg-game-bgCard border border-game-border text-game-text font-mono text-xs px-3 py-2 rounded w-36 focus:outline-none focus:border-game-primary placeholder-game-muted" />
          <label className="flex items-center gap-2 text-xs font-mono text-game-textDim cursor-pointer">
            <input type="checkbox" checked={live} onChange={(e: React.ChangeEvent<HTMLInputElement>) => setLive(e.target.checked)} className="accent-game-primary" />
            LIVE APPEND
          </label>
          <span className="text-xs text-game-muted ml-auto">{total.toLocaleString()} TOTAL EVENTS</span>
        </div>

        {/* Event feed */}
        <div className="flex-1 bg-game-bgCard border border-game-border rounded overflow-auto font-mono text-xs">
          <table className="w-full">
            <thead className="sticky top-0 bg-game-bgCard border-b border-game-border">
              <tr className="text-game-textDim">
                <th className="text-left px-4 py-2">TIMESTAMP</th>
                <th className="text-left px-4 py-2">TYPE</th>
                <th className="text-left px-4 py-2">ACCOUNT</th>
                <th className="text-left px-4 py-2">GAME</th>
                <th className="text-left px-4 py-2">DETAIL</th>
              </tr>
            </thead>
            <tbody>
              {events.map((ev, i) => (
                <tr key={i} className="border-b border-game-border last:border-0 hover:bg-game-bgHover">
                  <td className="px-4 py-1.5 text-game-muted whitespace-nowrap">
                    {new Date(ev.ts ?? ev.createdAt).toLocaleString()}
                  </td>
                  <td className={`px-4 py-1.5 ${TYPE_COLORS[ev.type] || 'text-game-textDim'}`}>{ev.type}</td>
                  <td className="px-4 py-1.5 text-game-text">{ev.accountId || (ev.data as Record<string, unknown>)?.accountId as string || '—'}</td>
                  <td className="px-4 py-1.5 text-game-textDim">{ev.gameId || (ev.data as Record<string, unknown>)?.gameId as string || '—'}</td>
                  <td className="px-4 py-1.5 text-game-muted truncate max-w-xs">
                    {(ev.data as Record<string, unknown>)?.name as string || ev.name || ''}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>

        </div>

        {/* Pagination */}
        <div className="flex items-center justify-between mt-3">
          <span className="text-xs font-mono text-game-muted">
            PAGE {page} OF {totalPages} &nbsp;·&nbsp; {PAGE_SIZE} PER PAGE
          </span>
          <div className="flex gap-2">
            <button onClick={() => setPage(1)} disabled={page === 1} className={btnClass(page === 1)}>«</button>
            <button onClick={() => setPage(p => p - 1)} disabled={page === 1} className={btnClass(page === 1)}>‹ PREV</button>
            <button onClick={() => setPage(p => p + 1)} disabled={page === totalPages} className={btnClass(page === totalPages)}>NEXT ›</button>
            <button onClick={() => setPage(totalPages)} disabled={page === totalPages} className={btnClass(page === totalPages)}>»</button>
          </div>
        </div>
      </main>
    </div>
  );
}
