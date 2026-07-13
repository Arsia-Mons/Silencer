'use client';
import { useEffect, useMemo, useState } from 'react';
import { useAuth } from '../../lib/auth';
import { useSocket } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import {
  getRoadmap, createRoadmapItem, updateRoadmapItem, deleteRoadmapItem,
  type RoadmapItem, type RoadmapStatus, type RoadmapEffort,
} from '../../lib/api';

const ROLE_RANK: Record<string, number> = { viewer: 0, moderator: 1, manager: 2, admin: 3, superadmin: 4 };
const MANAGER_RANK = 2;

function getMyRank(): number {
  if (typeof window === 'undefined') return -1;
  try {
    const payload = JSON.parse(atob(localStorage.getItem('zs_token')?.split('.')[1] || ''));
    return ROLE_RANK[payload.role as string] ?? -1;
  } catch { return -1; }
}

const SECTION_META: Record<string, { icon: string; label: string }> = {
  modes:     { icon: '◈', label: 'GAME MODES' },
  mechanics: { icon: '◉', label: 'MECHANICS' },
  weapons:   { icon: '⚔', label: 'WEAPONS' },
  npcs:      { icon: '☈', label: 'NPCS & SECURITY' },
  objects:   { icon: '⊟', label: 'OBJECT TYPES' },
  items:     { icon: '✦', label: 'ITEMS & ECONOMY' },
};

const STATUS_META: Record<RoadmapStatus, { label: string; cls: string; border: string }> = {
  'proposed':    { label: 'PROPOSED',    cls: 'text-game-muted border-game-border bg-game-dark',        border: 'border-game-border' },
  'designing':   { label: 'DESIGNING',   cls: 'text-sky-300 border-sky-500/40 bg-sky-500/10',           border: 'border-sky-500/60' },
  'in-progress': { label: 'IN PROGRESS', cls: 'text-amber-300 border-amber-500/40 bg-amber-500/10',     border: 'border-amber-500/60' },
  'shipped':     { label: 'SHIPPED',     cls: 'text-emerald-300 border-emerald-500/40 bg-emerald-500/10', border: 'border-emerald-500/60' },
};

const EFFORT_META: Record<RoadmapEffort, string> = { S: 'S · quick', M: 'M · medium', L: 'L · large' };

const BLANK = { section: 'modes', title: '', detail: '', buildsOn: '', status: 'proposed' as RoadmapStatus, effort: 'M' as RoadmapEffort };

export default function RoadmapPage() {
  useAuth();
  const wsConnected = useSocket({});
  const canEdit = useMemo(() => getMyRank() >= MANAGER_RANK, []);

  const [items, setItems] = useState<RoadmapItem[]>([]);
  const [sections, setSections] = useState<string[]>([]);
  const [statuses, setStatuses] = useState<RoadmapStatus[]>([]);
  const [efforts, setEfforts] = useState<RoadmapEffort[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [editingId, setEditingId] = useState<string | null>(null);
  const [form, setForm] = useState<typeof BLANK>({ ...BLANK });
  const [adding, setAdding] = useState(false);

  async function load() {
    try {
      const data = await getRoadmap();
      setItems(data.items);
      setSections(data.sections);
      setStatuses(data.statuses);
      setEfforts(data.efforts);
      setError(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to load roadmap');
    } finally {
      setLoading(false);
    }
  }
  useEffect(() => { load(); }, []);

  const grouped = useMemo(() => {
    const by: Record<string, RoadmapItem[]> = {};
    for (const s of sections) by[s] = [];
    for (const it of items) (by[it.section] ||= []).push(it);
    return by;
  }, [items, sections]);

  const counts = useMemo(() => {
    const c: Record<string, number> = {};
    for (const it of items) c[it.status] = (c[it.status] || 0) + 1;
    return c;
  }, [items]);

  async function patch(id: string, data: Partial<RoadmapItem>) {
    const updated = await updateRoadmapItem(id, data);
    setItems((prev) => prev.map((it) => (it._id === id ? updated : it)));
  }

  async function remove(id: string) {
    if (!confirm('Delete this roadmap item?')) return;
    await deleteRoadmapItem(id);
    setItems((prev) => prev.filter((it) => it._id !== id));
  }

  async function submitForm() {
    if (!form.title.trim()) return;
    if (editingId) {
      await patch(editingId, form);
    } else {
      const created = await createRoadmapItem(form);
      setItems((prev) => [...prev, created]);
    }
    setEditingId(null);
    setAdding(false);
    setForm({ ...BLANK });
  }

  function startEdit(it: RoadmapItem) {
    setEditingId(it._id);
    setAdding(false);
    setForm({ section: it.section, title: it.title, detail: it.detail, buildsOn: it.buildsOn, status: it.status, effort: it.effort });
  }

  function startAdd() {
    setEditingId(null);
    setAdding(true);
    setForm({ ...BLANK });
  }

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <div className="flex items-start justify-between mb-6">
          <div>
            <h1 className="text-game-primary font-mono text-xl tracking-widest mb-1">◇ ROADMAP</h1>
            <p className="text-game-textDim text-xs font-mono">
              {items.length} ITEMS · {counts['shipped'] || 0} SHIPPED · {counts['in-progress'] || 0} IN PROGRESS
              {!canEdit && <span className="text-game-muted"> · READ ONLY</span>}
            </p>
          </div>
          <div className="flex gap-2 items-center flex-wrap justify-end">
            {(Object.keys(STATUS_META) as RoadmapStatus[]).map((s) => (
              <span key={s} className={`inline-flex items-center gap-1 px-2 py-0.5 rounded text-[10px] font-mono border ${STATUS_META[s].cls}`}>
                {STATUS_META[s].label} {counts[s] ? `· ${counts[s]}` : ''}
              </span>
            ))}
            {canEdit && (
              <button onClick={startAdd}
                className="px-3 py-1 text-xs font-mono text-game-primary border border-game-primary/50 rounded hover:bg-game-primary/10">
                + ADD ITEM
              </button>
            )}
          </div>
        </div>

        {error && <div className="mb-4 text-xs font-mono text-game-danger">⚠ {error}</div>}
        {loading && <div className="text-xs font-mono text-game-textDim">Loading…</div>}

        {(adding || editingId) && canEdit && (
          <ItemForm
            form={form} setForm={setForm} sections={sections} statuses={statuses} efforts={efforts}
            isEdit={!!editingId} onSubmit={submitForm}
            onCancel={() => { setAdding(false); setEditingId(null); setForm({ ...BLANK }); }}
          />
        )}

        {sections.map((section) => {
          const list = grouped[section] || [];
          if (list.length === 0) return null;
          const meta = SECTION_META[section] || { icon: '•', label: section.toUpperCase() };
          return (
            <div key={section} className="mb-8">
              <div className="flex items-baseline gap-3 mb-3 pb-2 border-b border-game-border">
                <span className="text-game-primary font-mono font-bold tracking-widest">{meta.icon} {meta.label}</span>
                <span className="text-game-muted font-mono text-[10px]">{list.length}</span>
              </div>
              <div className="space-y-2">
                {list.map((it) => (
                  <div key={it._id} className="rounded border border-game-border bg-game-bgCard p-3 flex items-start gap-3">
                    <div className="flex-1 min-w-0">
                      <div className="flex items-center gap-2 flex-wrap">
                        <span className="text-game-text font-mono text-sm">{it.title}</span>
                        <span className="text-[10px] font-mono text-game-muted border border-game-border rounded px-1.5 py-0.5">{EFFORT_META[it.effort]}</span>
                      </div>
                      {it.detail && <p className="text-game-textDim font-mono text-xs mt-1 leading-relaxed">{it.detail}</p>}
                      {it.buildsOn && <p className="text-game-muted font-mono text-[10px] mt-1">builds on: {it.buildsOn}</p>}
                    </div>
                    <div className="flex items-center gap-2 shrink-0">
                      {canEdit ? (
                        <select value={it.status} onChange={(e) => patch(it._id, { status: e.target.value as RoadmapStatus })}
                          className={`text-[10px] font-mono font-bold tracking-wide rounded border px-1.5 py-1 bg-game-dark text-game-text [color-scheme:dark] ${STATUS_META[it.status].border}`}>
                          {statuses.map((s) => <option key={s} value={s} className="bg-game-dark text-game-text">{STATUS_META[s]?.label || s}</option>)}
                        </select>
                      ) : (
                        <span className={`inline-flex items-center px-2 py-0.5 rounded text-[10px] font-mono border ${STATUS_META[it.status].cls}`}>
                          {STATUS_META[it.status].label}
                        </span>
                      )}
                      {canEdit && (
                        <>
                          <button onClick={() => startEdit(it)} title="Edit"
                            className="text-game-muted hover:text-game-text text-xs font-mono px-1">✎</button>
                          <button onClick={() => remove(it._id)} title="Delete"
                            className="text-game-muted hover:text-game-danger text-xs font-mono px-1">✕</button>
                        </>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          );
        })}

        <p className="text-game-muted text-[10px] font-mono text-center mt-4 pb-2">
          SILENCER ADMIN CONSOLE — ROADMAP · SOURCE docs/roadmap.md
        </p>
      </main>
    </div>
  );
}

interface ItemFormProps {
  form: typeof BLANK;
  setForm: (f: typeof BLANK) => void;
  sections: string[];
  statuses: RoadmapStatus[];
  efforts: RoadmapEffort[];
  isEdit: boolean;
  onSubmit: () => void;
  onCancel: () => void;
}

function ItemForm({ form, setForm, sections, statuses, efforts, isEdit, onSubmit, onCancel }: ItemFormProps) {
  const input = 'w-full bg-game-dark border border-game-border rounded px-2 py-1 text-xs font-mono text-game-text [color-scheme:dark]';
  return (
    <div className="mb-6 rounded border border-game-primary/40 bg-game-bgCard p-4">
      <div className="text-game-primary font-mono text-xs tracking-widest mb-3">{isEdit ? '✎ EDIT ITEM' : '+ NEW ITEM'}</div>
      <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
        <label className="text-[10px] font-mono text-game-textDim">TITLE
          <input className={input} value={form.title} onChange={(e) => setForm({ ...form, title: e.target.value })} />
        </label>
        <label className="text-[10px] font-mono text-game-textDim">BUILDS ON
          <input className={input} value={form.buildsOn} onChange={(e) => setForm({ ...form, buildsOn: e.target.value })} />
        </label>
        <label className="text-[10px] font-mono text-game-textDim md:col-span-2">DETAIL
          <textarea className={input} rows={2} value={form.detail} onChange={(e) => setForm({ ...form, detail: e.target.value })} />
        </label>
        <label className="text-[10px] font-mono text-game-textDim">SECTION
          <select className={input} value={form.section} onChange={(e) => setForm({ ...form, section: e.target.value })}>
            {sections.map((s) => <option key={s} value={s}>{SECTION_META[s]?.label || s}</option>)}
          </select>
        </label>
        <div className="grid grid-cols-2 gap-3">
          <label className="text-[10px] font-mono text-game-textDim">STATUS
            <select className={input} value={form.status} onChange={(e) => setForm({ ...form, status: e.target.value as RoadmapStatus })}>
              {statuses.map((s) => <option key={s} value={s}>{STATUS_META[s]?.label || s}</option>)}
            </select>
          </label>
          <label className="text-[10px] font-mono text-game-textDim">EFFORT
            <select className={input} value={form.effort} onChange={(e) => setForm({ ...form, effort: e.target.value as RoadmapEffort })}>
              {efforts.map((s) => <option key={s} value={s}>{s}</option>)}
            </select>
          </label>
        </div>
      </div>
      <div className="flex gap-2 mt-3">
        <button onClick={onSubmit} disabled={!form.title.trim()}
          className="px-3 py-1 text-xs font-mono text-game-primary border border-game-primary/50 rounded hover:bg-game-primary/10 disabled:opacity-40">
          {isEdit ? 'SAVE' : 'CREATE'}
        </button>
        <button onClick={onCancel}
          className="px-3 py-1 text-xs font-mono text-game-muted border border-game-border rounded hover:text-game-text">
          CANCEL
        </button>
      </div>
    </div>
  );
}
