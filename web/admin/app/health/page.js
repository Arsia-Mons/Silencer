'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import StatCard from '../../components/StatCard.js';
import { useState, useEffect, useCallback } from 'react';
import { getStats, triggerBackup, getBackupStatus, listBackups } from '../../lib/api.js';

export default function Health() {
  useAuth();
  const [stats, setStats]       = useState(null);
  const [error, setError]       = useState(null);
  const [backups, setBackups]   = useState([]);
  const [bkStatus, setBkStatus] = useState(null);
  const [bkTriggered, setBkTriggered] = useState(false);

  const load = async () => {
    try { setStats(await getStats()); setError(null); }
    catch (e) { setError(e.message); }
  };

  const loadBackups = useCallback(async () => {
    try {
      const [listRes, statusRes] = await Promise.all([listBackups(), getBackupStatus()]);
      setBackups(listRes.files || []);
      setBkStatus(statusRes);
    } catch { /* ignore */ }
  }, []);

  useEffect(() => {
    load();
    loadBackups();
    const t1 = setInterval(load, 5000);
    const t2 = setInterval(loadBackups, 5000);
    return () => { clearInterval(t1); clearInterval(t2); };
  }, [loadBackups]);

  // Poll faster while backup is in progress
  useEffect(() => {
    if (!bkStatus?.inProgress) return;
    const t = setInterval(loadBackups, 2000);
    return () => clearInterval(t);
  }, [bkStatus?.inProgress, loadBackups]);

  const handleBackup = async () => {
    setBkTriggered(true);
    try {
      await triggerBackup();
      await loadBackups();
    } catch (e) {
      setError(e.message);
    } finally {
      setBkTriggered(false);
    }
  };

  const wsConnected = useSocket({});

  const StatusDot = ({ ok }) => (
    <span className={`inline-block w-2.5 h-2.5 rounded-full mr-2 ${ok ? 'bg-game-primary' : 'bg-game-danger'}`} />
  );

  const fmtDate = (iso) => iso ? new Date(iso).toLocaleString() : '—';

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
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
              <StatCard label="ONLINE PLAYERS" value={stats.lobby.onlinePlayers} color="primary" />
              <StatCard label="ACTIVE GAMES"   value={stats.lobby.activeGames}   color="info" />
              <StatCard label="TOTAL PLAYERS"  value={stats.db.totalPlayers}     color="primary" />
              <StatCard label="TOTAL EVENTS"   value={stats.db.totalEvents}      color="primary" />
            </div>

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

            <h2 className="text-xs text-game-textDim tracking-widest mb-3">DASHBOARD CONNECTION</h2>
            <div className="bg-game-bgCard border border-game-border rounded p-4 flex items-center gap-3 w-fit mb-8">
              <StatusDot ok={wsConnected} />
              <div>
                <div className="text-xs font-mono text-game-text">WEBSOCKET</div>
                <div className="text-xs font-mono text-game-muted">{wsConnected ? 'LIVE' : 'DISCONNECTED'}</div>
              </div>
            </div>
          </>
        )}

        {/* ── BACKUP PANEL ── */}
        <h2 className="text-xs text-game-textDim tracking-widest mb-3">DATABASE BACKUP</h2>
        <div className="bg-game-bgCard border border-game-border rounded p-4 mb-4">
          <div className="flex items-center justify-between mb-3">
            <div>
              <div className="text-xs font-mono text-game-text mb-1">AUTO-BACKUP</div>
              <div className="text-xs font-mono text-game-muted">
                Every 6 hours · Keep last 10
                {bkStatus?.githubConfigured
                  ? <span className="text-game-primary ml-2">· GitHub ✓</span>
                  : <span className="text-game-muted ml-2">· GitHub (not configured)</span>}
              </div>
            </div>
            <button
              onClick={handleBackup}
              disabled={bkTriggered || bkStatus?.inProgress}
              className="px-4 py-2 text-xs font-mono rounded border border-game-primary text-game-primary hover:bg-game-primary hover:text-black transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
            >
              {bkStatus?.inProgress ? '⏳ BACKING UP…' : '⬇ BACKUP NOW'}
            </button>
          </div>

          {bkStatus?.lastResult && (
            <div className={`text-xs font-mono px-3 py-2 rounded mb-3 ${bkStatus.lastResult.ok ? 'bg-green-900/20 text-game-primary border border-game-primary/30' : 'bg-red-900/20 text-game-danger border border-game-danger/30'}`}>
              {bkStatus.lastResult.ok ? (
                <span>
                  ✓ Last backup: {bkStatus.lastResult.filename} — {bkStatus.lastResult.sizeKB} KB — {fmtDate(bkStatus.lastResult.ts)}
                  {bkStatus.lastResult.githubUrl && (
                    <> · <a href={bkStatus.lastResult.githubUrl} target="_blank" rel="noreferrer" className="underline">GitHub ↗</a></>
                  )}
                  {bkStatus.lastResult.githubError && (
                    <span className="text-game-danger ml-2">⚠ GitHub: {bkStatus.lastResult.githubError}</span>
                  )}
                </span>
              ) : `✗ Last backup failed: ${bkStatus.lastResult.error}`}
            </div>
          )}

          {backups.length > 0 ? (
            <table className="w-full text-xs font-mono">
              <thead>
                <tr className="text-game-muted border-b border-game-border">
                  <th className="text-left pb-2">FILENAME</th>
                  <th className="text-right pb-2">SIZE</th>
                  <th className="text-right pb-2">DATE</th>
                </tr>
              </thead>
              <tbody>
                {backups.map((b) => (
                  <tr key={b.filename} className="border-b border-game-border/30 text-game-text">
                    <td className="py-1.5 text-game-muted truncate max-w-xs">{b.filename}</td>
                    <td className="py-1.5 text-right">{b.sizeKB} KB</td>
                    <td className="py-1.5 text-right">{fmtDate(b.ts)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <div className="text-xs font-mono text-game-muted">No backups yet.</div>
          )}
        </div>

        <div className="text-xs font-mono text-game-muted">
          Backups stored in Docker volume <code className="text-game-primary">backup-data</code>.
          To restore: <code className="text-game-primary">mongorestore --uri=mongodb://localhost:28017/silencer --archive=&lt;file&gt; --gzip</code>
        </div>
      </main>
    </div>
  );
}
