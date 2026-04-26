'use client';
import { useState } from 'react';
import { useRouter, useSearchParams } from 'next/navigation';
import { login, playerLogin } from '../../lib/api';
import { Suspense } from 'react';

function LoginForm() {
  const router = useRouter();
  const searchParams = useSearchParams();
  const [mode, setMode]     = useState<'admin' | 'player'>(searchParams.get('mode') === 'player' ? 'player' : 'admin');
  const [form, setForm]     = useState({ username: '', password: '' });
  const [error, setError]   = useState('');
  const [loading, setLoading] = useState(false);

  const isPlayer = mode === 'player';

  const handleSubmit = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    setLoading(true);
    setError('');
    try {
      if (isPlayer) {
        const res = await playerLogin(form.username, form.password) as { token: string; accountId: string; name: string };
        localStorage.setItem('zs_player_token', res.token);
        localStorage.setItem('zs_player', JSON.stringify({ accountId: res.accountId, name: res.name }));
        router.replace('/me');
      } else {
        const res = await login(form.username, form.password) as { token: string; role: string; username: string };
        localStorage.setItem('zs_token', res.token);
        localStorage.setItem('zs_user', JSON.stringify({ username: res.username, role: res.role }));
        router.replace('/dashboard');
      }
    } catch {
      setError(isPlayer ? 'ACCESS DENIED — CHECK YOUR GAME CREDENTIALS' : 'ACCESS DENIED — INVALID CREDENTIALS');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center">
      <div className="w-full max-w-sm">
        <div className="flex justify-center mb-8">
          <img src="/logo.png" alt="Silencer" className="h-20 w-auto" />
        </div>

        {/* Mode tabs */}
        <div className="flex mb-4 border border-game-border rounded overflow-hidden">
          {(['admin', 'player'] as const).map((m) => (
            <button key={m} onClick={() => { setMode(m); setError(''); }}
              className={`flex-1 py-2 text-xs font-mono tracking-widest transition-colors
                ${mode === m
                  ? 'bg-game-dark text-game-primary border-b-2 border-game-primary'
                  : 'text-game-textDim hover:text-game-text bg-game-bgCard'}`}>
              {m === 'admin' ? '⬡ ADMIN' : '◈ PLAYER'}
            </button>
          ))}
        </div>

        <div className="bg-game-bgCard border border-game-primary rounded p-6">
          <h1 className="text-game-primary font-mono text-sm tracking-widest mb-1 text-center">
            {isPlayer ? 'PLAYER PORTAL' : 'ADMIN CONSOLE ACCESS'}
          </h1>
          {isPlayer && (
            <p className="text-game-textDim text-xs font-mono text-center mb-4">
              Use your in-game callsign and password
            </p>
          )}
          <form onSubmit={handleSubmit} className="space-y-4 mt-4">
            <div>
              <label className="text-xs text-game-textDim font-mono block mb-1">CALLSIGN</label>
              <input type="text" value={form.username}
                onChange={(e: React.ChangeEvent<HTMLInputElement>) => setForm(f => ({ ...f, username: e.target.value }))}
                required autoFocus
                className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-primary" />
            </div>
            <div>
              <label className="text-xs text-game-textDim font-mono block mb-1">PASSPHRASE</label>
              <input type="password" value={form.password}
                onChange={(e: React.ChangeEvent<HTMLInputElement>) => setForm(f => ({ ...f, password: e.target.value }))}
                required
                className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-primary" />
            </div>
            {error && <div className="text-game-danger text-xs font-mono">{error}</div>}
            <button type="submit" disabled={loading}
              className="w-full bg-game-dark border border-game-primary text-game-primary font-mono text-sm py-2 rounded hover:bg-game-primary hover:text-black transition-colors disabled:opacity-50">
              {loading ? 'AUTHENTICATING...' : '[ AUTHENTICATE ]'}
            </button>
          </form>
        </div>

        <p className="text-game-muted text-xs font-mono text-center mt-4">
          {isPlayer
            ? <span>Admin? <button onClick={() => setMode('admin')} className="text-game-textDim hover:text-game-primary underline">Admin login →</button></span>
            : <span>Player? <button onClick={() => setMode('player')} className="text-game-textDim hover:text-game-primary underline">Player portal →</button></span>}
        </p>
      </div>
    </div>
  );
}

export default function Login() {
  return <Suspense><LoginForm /></Suspense>;
}
