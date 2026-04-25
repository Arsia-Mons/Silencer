'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import { useState, useEffect, useCallback } from 'react';
import {
  getAdminUsers, createAdminUser, updateAdminUser,
  resetAdminPassword, deleteAdminUser, changeOwnPassword,
} from '../../lib/api.js';

// Role hierarchy — must match api/src/auth/jwt.js
const ROLE_RANK = { viewer: 0, moderator: 1, manager: 2, admin: 3, superadmin: 4 };
const ROLES = ['viewer', 'moderator', 'manager', 'admin', 'superadmin'];

const ROLE_COLORS = {
  superadmin: 'text-yellow-400 border-yellow-600',
  admin:      'text-game-danger border-red-800',
  manager:    'text-orange-400 border-orange-700',
  moderator:  'text-game-primary border-game-dark',
  viewer:     'text-game-muted border-game-border',
};

const ROLE_BADGE = {
  superadmin: 'bg-yellow-900/40 text-yellow-400 border border-yellow-700',
  admin:      'bg-red-900/40 text-game-danger border border-red-800',
  manager:    'bg-orange-900/40 text-orange-400 border border-orange-800',
  moderator:  'bg-game-dark text-game-primary border border-game-dark',
  viewer:     'bg-black/40 text-game-muted border border-game-border',
};

function myRank() {
  if (typeof window === 'undefined') return -1;
  try {
    const payload = JSON.parse(atob(localStorage.getItem('zs_token')?.split('.')[1] || ''));
    return ROLE_RANK[payload.role] ?? -1;
  } catch { return -1; }
}

function canCreate(targetRole) { return myRank() > (ROLE_RANK[targetRole] ?? 99); }
function canManage(targetRole)  { return myRank() > (ROLE_RANK[targetRole] ?? 99); }

const EMPTY_FORM = { username: '', password: '', role: 'moderator' };

export default function Users() {
  useAuth();
  const wsConnected = useSocket({});
  const [users, setUsers]       = useState([]);
  const [loading, setLoading]   = useState(false);
  const [error, setError]       = useState('');
  const [showCreate, setShowCreate] = useState(false);
  const [form, setForm]         = useState(EMPTY_FORM);
  const [formErr, setFormErr]   = useState('');
  const [saving, setSaving]     = useState(false);
  // Edit role modal
  const [editTarget, setEditTarget] = useState(null);
  const [editRole, setEditRole] = useState('');
  // Reset password modal
  const [pwTarget, setPwTarget] = useState(null);
  const [newPw, setNewPw]       = useState('');
  const [pwErr, setPwErr]       = useState('');
  // Change own password
  const [showMyPw, setShowMyPw]     = useState(false);
  const [myCurrentPw, setMyCurrentPw] = useState('');
  const [myNewPw, setMyNewPw]         = useState('');
  const [myConfirmPw, setMyConfirmPw] = useState('');
  const [myPwErr, setMyPwErr]         = useState('');
  const [myPwOk, setMyPwOk]           = useState(false);

  const rank = myRank();

  const load = useCallback(async () => {
    setLoading(true);
    try { setUsers(await getAdminUsers()); }
    catch (e) { setError(e.message); }
    finally { setLoading(false); }
  }, []);

  useEffect(() => { load(); }, [load]);

  // Create user
  async function handleCreate(e) {
    e.preventDefault();
    setFormErr('');
    if (!form.username.trim()) return setFormErr('Username required');
    if (form.password.length < 6) return setFormErr('Password must be ≥ 6 characters');
    if (!canCreate(form.role)) return setFormErr('You cannot create a user with that role');
    setSaving(true);
    try {
      await createAdminUser(form);
      setShowCreate(false);
      setForm(EMPTY_FORM);
      load();
    } catch (e) { setFormErr(e.message); }
    finally { setSaving(false); }
  }

  // Edit role
  async function handleEditRole() {
    if (!editTarget) return;
    setSaving(true);
    try {
      await updateAdminUser(editTarget._id, { role: editRole });
      setEditTarget(null);
      load();
    } catch (e) { setFormErr(e.message); }
    finally { setSaving(false); }
  }

  // Reset password
  async function handleResetPw() {
    setPwErr('');
    if (newPw.length < 6) return setPwErr('Password must be ≥ 6 characters');
    setSaving(true);
    try {
      await resetAdminPassword(pwTarget._id, newPw);
      setPwTarget(null);
      setNewPw('');
    } catch (e) { setPwErr(e.message); }
    finally { setSaving(false); }
  }

  // Change own password
  async function handleChangeMyPw(e) {
    e.preventDefault();
    setMyPwErr('');
    setMyPwOk(false);
    if (myNewPw.length < 6) return setMyPwErr('Password must be ≥ 6 characters');
    if (myNewPw !== myConfirmPw) return setMyPwErr('Passwords do not match');
    setSaving(true);
    try {
      await changeOwnPassword(myCurrentPw, myNewPw);
      setMyPwOk(true);
      setMyCurrentPw(''); setMyNewPw(''); setMyConfirmPw('');
    } catch (e) { setMyPwErr(e.message); }
    finally { setSaving(false); }
  }

  // Delete
  async function handleDelete(user) {
    if (!window.confirm(`Delete user "${user.username}"?`)) return;
    try {
      await deleteAdminUser(user._id);
      load();
    } catch (e) { setError(e.message); }
  }

  // Roles the current user can assign (must outrank target role)
  const creatableRoles = ROLES.filter(r => canCreate(r));

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-game-primary font-mono text-xl tracking-widest">⬡ USER MANAGEMENT</h1>
          {creatableRoles.length > 0 && (
            <button onClick={() => { setShowCreate(true); setForm({ ...EMPTY_FORM, role: creatableRoles[0] }); }}
              className="px-4 py-2 text-xs font-mono border border-game-primary text-game-primary hover:bg-game-dark rounded transition-colors">
              + CREATE USER
            </button>
          )}
          <button onClick={() => { setShowMyPw(true); setMyCurrentPw(''); setMyNewPw(''); setMyConfirmPw(''); setMyPwErr(''); setMyPwOk(false); }}
            className="px-4 py-2 text-xs font-mono border border-game-border text-game-textDim hover:border-game-warning hover:text-game-warning rounded transition-colors">
            🔑 MY PASSWORD
          </button>
        </div>

        {error && <div className="text-game-danger text-xs font-mono mb-4">{error}</div>}

        {/* Role legend */}
        <div className="flex gap-3 mb-5 flex-wrap">
          {ROLES.slice().reverse().map(r => (
            <span key={r} className={`px-2 py-0.5 text-xs font-mono rounded ${ROLE_BADGE[r]}`}>
              {r.toUpperCase()}
            </span>
          ))}
        </div>

        {/* Users table */}
        <div className="bg-game-bgCard border border-game-border rounded overflow-hidden">
          <table className="w-full text-xs font-mono">
            <thead>
              <tr className="border-b border-game-border text-game-textDim">
                <th className="text-left px-4 py-3">USERNAME</th>
                <th className="text-left px-4 py-3">ROLE</th>
                <th className="text-left px-4 py-3">CREATED BY</th>
                <th className="text-left px-4 py-3">CREATED</th>
                <th className="text-left px-4 py-3">ACTIONS</th>
              </tr>
            </thead>
            <tbody>
              {loading && (
                <tr><td colSpan={5} className="px-4 py-6 text-center text-game-textDim animate-pulse">LOADING...</td></tr>
              )}
              {!loading && users.map(u => {
                const manageable = canManage(u.role);
                return (
                  <tr key={u._id} className="border-b border-game-border last:border-0 hover:bg-game-bgHover">
                    <td className="px-4 py-3 text-game-text">{u.username}</td>
                    <td className="px-4 py-3">
                      <span className={`px-2 py-0.5 rounded ${ROLE_BADGE[u.role]}`}>{u.role.toUpperCase()}</span>
                    </td>
                    <td className="px-4 py-3 text-game-muted">{u.createdBy || '—'}</td>
                    <td className="px-4 py-3 text-game-muted">
                      {u.createdAt ? new Date(u.createdAt).toLocaleDateString() : '—'}
                    </td>
                    <td className="px-4 py-3">
                      {manageable ? (
                        <div className="flex gap-2">
                          <button
                            onClick={() => { setEditTarget(u); setEditRole(u.role); setFormErr(''); }}
                            className="px-2 py-1 border border-game-border text-game-textDim hover:border-game-primary hover:text-game-primary rounded transition-colors">
                            EDIT ROLE
                          </button>
                          <button
                            onClick={() => { setPwTarget(u); setNewPw(''); setPwErr(''); }}
                            className="px-2 py-1 border border-game-border text-game-textDim hover:border-game-warning hover:text-game-warning rounded transition-colors">
                            RESET PW
                          </button>
                          <button
                            onClick={() => handleDelete(u)}
                            className="px-2 py-1 border border-game-border text-game-textDim hover:border-game-danger hover:text-game-danger rounded transition-colors">
                            DELETE
                          </button>
                        </div>
                      ) : (
                        <span className="text-game-muted">—</span>
                      )}
                    </td>
                  </tr>
                );
              })}
              {!loading && users.length === 0 && (
                <tr><td colSpan={5} className="px-4 py-6 text-center text-game-muted">NO USERS FOUND</td></tr>
              )}
            </tbody>
          </table>
        </div>

        {/* CREATE USER MODAL */}
        {showCreate && (
          <div className="fixed inset-0 bg-black/70 flex items-center justify-center z-50"
            onClick={() => setShowCreate(false)}>
            <div className="bg-game-bgCard border border-game-primary rounded p-6 w-full max-w-md"
              onClick={e => e.stopPropagation()}>
              <div className="flex justify-between items-center mb-5">
                <h2 className="text-game-primary font-mono text-base tracking-widest">// CREATE USER</h2>
                <button onClick={() => setShowCreate(false)} className="text-game-muted hover:text-game-text">✕</button>
              </div>
              <form onSubmit={handleCreate} className="flex flex-col gap-4">
                <div>
                  <label className="block text-xs text-game-textDim mb-1">USERNAME</label>
                  <input value={form.username} onChange={e => setForm(f => ({ ...f, username: e.target.value }))}
                    className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-primary"
                    autoComplete="off" />
                </div>
                <div>
                  <label className="block text-xs text-game-textDim mb-1">PASSWORD</label>
                  <input type="password" value={form.password} onChange={e => setForm(f => ({ ...f, password: e.target.value }))}
                    className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-primary"
                    autoComplete="new-password" />
                </div>
                <div>
                  <label className="block text-xs text-game-textDim mb-1">ROLE</label>
                  <select value={form.role} onChange={e => setForm(f => ({ ...f, role: e.target.value }))}
                    className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-primary">
                    {creatableRoles.map(r => (
                      <option key={r} value={r}>{r.toUpperCase()}</option>
                    ))}
                  </select>
                </div>
                {formErr && <div className="text-game-danger text-xs">{formErr}</div>}
                <div className="flex gap-3 mt-2">
                  <button type="submit" disabled={saving}
                    className="flex-1 py-2 text-xs font-mono border border-game-primary text-game-primary hover:bg-game-dark rounded transition-colors disabled:opacity-50">
                    {saving ? 'CREATING...' : 'CREATE'}
                  </button>
                  <button type="button" onClick={() => setShowCreate(false)}
                    className="flex-1 py-2 text-xs font-mono border border-game-border text-game-textDim hover:border-game-danger hover:text-game-danger rounded transition-colors">
                    CANCEL
                  </button>
                </div>
              </form>
            </div>
          </div>
        )}

        {/* EDIT ROLE MODAL */}
        {editTarget && (
          <div className="fixed inset-0 bg-black/70 flex items-center justify-center z-50"
            onClick={() => setEditTarget(null)}>
            <div className="bg-game-bgCard border border-game-primary rounded p-6 w-full max-w-sm"
              onClick={e => e.stopPropagation()}>
              <div className="flex justify-between items-center mb-5">
                <h2 className="text-game-primary font-mono text-base tracking-widest">// EDIT ROLE</h2>
                <button onClick={() => setEditTarget(null)} className="text-game-muted hover:text-game-text">✕</button>
              </div>
              <p className="text-xs text-game-textDim mb-4">
                Editing <span className="text-game-text">{editTarget.username}</span> — current role:{' '}
                <span className={ROLE_COLORS[editTarget.role]}>{editTarget.role.toUpperCase()}</span>
              </p>
              <select value={editRole} onChange={e => setEditRole(e.target.value)}
                className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-primary mb-4">
                {creatableRoles.map(r => (
                  <option key={r} value={r}>{r.toUpperCase()}</option>
                ))}
              </select>
              {formErr && <div className="text-game-danger text-xs mb-3">{formErr}</div>}
              <div className="flex gap-3">
                <button onClick={handleEditRole} disabled={saving || editRole === editTarget.role}
                  className="flex-1 py-2 text-xs font-mono border border-game-primary text-game-primary hover:bg-game-dark rounded transition-colors disabled:opacity-50">
                  {saving ? 'SAVING...' : 'SAVE'}
                </button>
                <button onClick={() => setEditTarget(null)}
                  className="flex-1 py-2 text-xs font-mono border border-game-border text-game-textDim hover:border-game-danger hover:text-game-danger rounded transition-colors">
                  CANCEL
                </button>
              </div>
            </div>
          </div>
        )}

        {/* RESET PASSWORD MODAL */}
        {pwTarget && (
          <div className="fixed inset-0 bg-black/70 flex items-center justify-center z-50"
            onClick={() => setPwTarget(null)}>
            <div className="bg-game-bgCard border border-game-warning rounded p-6 w-full max-w-sm"
              onClick={e => e.stopPropagation()}>
              <div className="flex justify-between items-center mb-5">
                <h2 className="text-game-warning font-mono text-base tracking-widest">// RESET PASSWORD</h2>
                <button onClick={() => setPwTarget(null)} className="text-game-muted hover:text-game-text">✕</button>
              </div>
              <p className="text-xs text-game-textDim mb-4">
                Setting new password for <span className="text-game-text">{pwTarget.username}</span>
              </p>
              <input type="password" value={newPw} onChange={e => setNewPw(e.target.value)}
                placeholder="New password (min 6 chars)"
                className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-warning mb-2"
                autoComplete="new-password" />
              {pwErr && <div className="text-game-danger text-xs mb-3">{pwErr}</div>}
              <div className="flex gap-3 mt-3">
                <button onClick={handleResetPw} disabled={saving}
                  className="flex-1 py-2 text-xs font-mono border border-game-warning text-game-warning hover:bg-yellow-900/20 rounded transition-colors disabled:opacity-50">
                  {saving ? 'RESETTING...' : 'RESET'}
                </button>
                <button onClick={() => setPwTarget(null)}
                  className="flex-1 py-2 text-xs font-mono border border-game-border text-game-textDim hover:border-game-danger hover:text-game-danger rounded transition-colors">
                  CANCEL
                </button>
              </div>
            </div>
          </div>
        )}
        {/* CHANGE MY PASSWORD MODAL */}
        {showMyPw && (
          <div className="fixed inset-0 bg-black/70 flex items-center justify-center z-50"
            onClick={() => setShowMyPw(false)}>
            <div className="bg-game-bgCard border border-game-warning rounded p-6 w-full max-w-sm"
              onClick={e => e.stopPropagation()}>
              <div className="flex justify-between items-center mb-5">
                <h2 className="text-game-warning font-mono text-base tracking-widest">// CHANGE MY PASSWORD</h2>
                <button onClick={() => setShowMyPw(false)} className="text-game-muted hover:text-game-text">✕</button>
              </div>
              <form onSubmit={handleChangeMyPw} className="flex flex-col gap-3">
                <div>
                  <label className="block text-xs text-game-textDim mb-1">CURRENT PASSWORD</label>
                  <input type="password" value={myCurrentPw} onChange={e => setMyCurrentPw(e.target.value)}
                    className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-warning"
                    autoComplete="current-password" />
                </div>
                <div>
                  <label className="block text-xs text-game-textDim mb-1">NEW PASSWORD</label>
                  <input type="password" value={myNewPw} onChange={e => setMyNewPw(e.target.value)}
                    className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-warning"
                    autoComplete="new-password" />
                </div>
                <div>
                  <label className="block text-xs text-game-textDim mb-1">CONFIRM NEW PASSWORD</label>
                  <input type="password" value={myConfirmPw} onChange={e => setMyConfirmPw(e.target.value)}
                    className="w-full bg-game-bg border border-game-border text-game-text font-mono text-sm px-3 py-2 rounded focus:outline-none focus:border-game-warning"
                    autoComplete="new-password" />
                </div>
                {myPwErr && <div className="text-game-danger text-xs">{myPwErr}</div>}
                {myPwOk  && <div className="text-game-primary text-xs">✓ Password changed successfully</div>}
                <div className="flex gap-3 mt-2">
                  <button type="submit" disabled={saving}
                    className="flex-1 py-2 text-xs font-mono border border-game-warning text-game-warning hover:bg-yellow-900/20 rounded transition-colors disabled:opacity-50">
                    {saving ? 'SAVING...' : 'CHANGE PASSWORD'}
                  </button>
                  <button type="button" onClick={() => setShowMyPw(false)}
                    className="flex-1 py-2 text-xs font-mono border border-game-border text-game-textDim hover:border-game-danger hover:text-game-danger rounded transition-colors">
                    CANCEL
                  </button>
                </div>
              </form>
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
