'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import StatCard from '../../components/StatCard.js';
import { useState, useEffect } from 'react';
import { getStats } from '../../lib/api.js';

export default function Health() {
  useAuth();
  const [stats, setStats]   = useState(null);
  const [error, setError]   = useState(null);

  const load = async () => {
    try { setStats(await getStats()); setError(null); }
    catch (e) { setError(e.message); }
  };

  useEffect(() => { load(); const t = setInterval(load, 5000); return () => clearInterval(t); }, []);

  const wsConnected = useSocket({});

  const StatusDot = ({ ok }) => (
    <span className={`inline-block w-2.5 h-2.5 rounded-full mr-2 ${ok ? 'bg-game-primary' : 'bg-game-danger'}`} />
  );

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-game-primary font-mono text-xl tracking-widest">◎ SERVER HEALTH</h1>
          <span className="text-xs text-game-muted font-mono">AUTO-REFRESH 5s</span>
        </div>

        {error && <div className="bg-red-900/20 border border-game-danger text-game-danger text-xs font-mono px-4 py-2 rounded mb-4">{error}</div>}

        {stats && (
          <>
            {/* Live stats */}
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
              <StatCard label="ONLINE PLAYERS" value={stats.lobby.onlinePlayers} color="primary" />
              <StatCard label="ACTIVE GAMES"   value={stats.lobby.activeGames}   color="info" />
              <StatCard label="TOTAL PLAYERS"  value={stats.db.totalPlayers}     color="primary" />
              <StatCard label="TOTAL EVENTS"   value={stats.db.totalEvents}      color="primary" />
            </div>

            {/* Service status */}
            <h2 className="text-xs text-game-textDim tracking-widest mb-3">SERVICE STATUS</h2>
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 mb-8">
              {[
                { label: 'LOBBY SERVER',  ok: true,                                    detail: 'TCP :15170' },
                { label: 'MONGODB',       ok: stats.db.status === 'connected',          detail: stats.db.status.toUpperCase() },
                { label: 'RABBITMQ',      ok: stats.rabbitmq.status === 'connected',    detail: stats.rabbitmq.status.toUpperCase() },
              ].map(({ label, ok, detail }) => (
                <div key={label} className="bg-game-bgCard border border-game-border rounded p-4 flex items-center gap-3">
                  <StatusDot ok={ok} />
                  <div>
                    <div className="text-xs font-mono text-game-text">{label}</div>
                    <div className="text-xs font-mono text-game-muted">{detail}</div>
                  </div>
                </div>
              ))}
            </div>

            {/* WebSocket status */}
            <h2 className="text-xs text-game-textDim tracking-widest mb-3">DASHBOARD CONNECTION</h2>
            <div className="bg-game-bgCard border border-game-border rounded p-4 flex items-center gap-3 w-fit">
              <StatusDot ok={wsConnected} />
              <div>
                <div className="text-xs font-mono text-game-text">WEBSOCKET</div>
                <div className="text-xs font-mono text-game-muted">{wsConnected ? 'LIVE' : 'DISCONNECTED'}</div>
              </div>
            </div>
          </>
        )}
      </main>
    </div>
  );
}
