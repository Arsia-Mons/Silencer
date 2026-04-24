'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import { useState, useEffect, useRef } from 'react';
import { getEvents } from '../../lib/api.js';

const TYPE_COLORS = {
  'player.login':   'text-game-primary',
  'player.logout':  'text-game-textDim',
  'game.created':   'text-game-info',
  'game.ready':     'text-game-warning',
  'game.ended':     'text-game-muted',
};

const EVENT_TYPES = ['', 'player.login', 'player.logout', 'game.created', 'game.ready', 'game.ended'];

export default function Audit() {
  useAuth();
  const [events, setEvents] = useState([]);
  const [total,  setTotal]  = useState(0);
  const [filter, setFilter] = useState({ type: '', accountId: '' });
  const [live,   setLive]   = useState(true);
  const bottomRef = useRef(null);

  const load = async () => {
    try {
      const params = { limit: 100 };
      if (filter.type) params.type = filter.type;
      if (filter.accountId) params.accountId = filter.accountId;
      const res = await getEvents(params);
      setEvents(res.events.reverse());
      setTotal(res.total);
    } catch (e) { console.error(e); }
  };

  useEffect(() => { load(); }, [filter]);

  const wsConnected = useSocket({
    'player.login':   (d) => { if (live) setEvents(prev => [...prev, { type: 'player.login',  ...d, ts: new Date() }]); },
    'player.logout':  (d) => { if (live) setEvents(prev => [...prev, { type: 'player.logout', ...d, ts: new Date() }]); },
    'game.created':   (d) => { if (live) setEvents(prev => [...prev, { type: 'game.created',  ...d, ts: new Date() }]); },
    'game.ready':     (d) => { if (live) setEvents(prev => [...prev, { type: 'game.ready',    ...d, ts: new Date() }]); },
    'game.ended':     (d) => { if (live) setEvents(prev => [...prev, { type: 'game.ended',    ...d, ts: new Date() }]); },
  });

  useEffect(() => {
    if (live) bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [events, live]);

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 flex flex-col overflow-hidden">
        <h1 className="text-game-primary font-mono text-xl tracking-widest mb-4">◧ AUDIT LOG</h1>

        {/* Filters */}
        <div className="flex flex-wrap gap-3 mb-4 items-center">
          <select value={filter.type} onChange={e => setFilter(f => ({ ...f, type: e.target.value }))}
            className="bg-game-bgCard border border-game-border text-game-text font-mono text-xs px-3 py-2 rounded focus:outline-none focus:border-game-primary">
            {EVENT_TYPES.map(t => <option key={t} value={t}>{t || 'ALL TYPES'}</option>)}
          </select>
          <input value={filter.accountId} onChange={e => setFilter(f => ({ ...f, accountId: e.target.value }))}
            placeholder="ACCOUNT ID..."
            className="bg-game-bgCard border border-game-border text-game-text font-mono text-xs px-3 py-2 rounded w-36 focus:outline-none focus:border-game-primary placeholder-game-muted" />
          <label className="flex items-center gap-2 text-xs font-mono text-game-textDim cursor-pointer">
            <input type="checkbox" checked={live} onChange={e => setLive(e.target.checked)} className="accent-game-primary" />
            LIVE APPEND
          </label>
          <span className="text-xs text-game-muted">{total} TOTAL EVENTS</span>
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
                    {new Date(ev.ts).toLocaleTimeString()}
                  </td>
                  <td className={`px-4 py-1.5 ${TYPE_COLORS[ev.type] || 'text-game-textDim'}`}>{ev.type}</td>
                  <td className="px-4 py-1.5 text-game-text">{ev.accountId || ev.data?.accountId || '—'}</td>
                  <td className="px-4 py-1.5 text-game-textDim">{ev.gameId || ev.data?.gameId || '—'}</td>
                  <td className="px-4 py-1.5 text-game-muted truncate max-w-xs">
                    {ev.data?.name || ev.name || ''}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
          <div ref={bottomRef} />
        </div>
      </main>
    </div>
  );
}
