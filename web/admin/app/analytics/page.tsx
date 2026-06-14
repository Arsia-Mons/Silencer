'use client';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { getAnalyticsMaps, getAnalyticsWeapons, getAnalyticsAgencies, getAnalyticsHeatmap } from '../../lib/api';
import { useState, useEffect, useRef, useCallback } from 'react';

const AGENCY_NAMES  = ['Noxis', 'Lazarus', 'Caliber', 'Static', 'Black Rose'];
const WEAPON_NAMES  = ['Blaster', 'Laser', 'Rocket', 'Flamer'];
const AGENCY_COLORS = ['#22c55e', '#22d3ee', '#facc15', '#a855f7', '#f43f5e'];
const WEAPON_COLORS = ['#38bdf8', '#f472b6', '#fb923c', '#a3e635'];

type Tab = 'heatmap' | 'weapons' | 'agencies';

interface HeatCell { gx: number; gy: number; count: number; }
interface WeaponRow { name: string; fires: number; hits: number; kills: number; accuracy: number; }
interface AgencyRow { name: string; wins: number; losses: number; winRate: number; agencyIdx: number; }

function heatColor(t: number): string {
  // cool (blue) → warm (yellow) → hot (red)
  const r = Math.min(255, Math.round(t < 0.5 ? t * 2 * 255 : 255));
  const g = Math.min(255, Math.round(t < 0.5 ? 0 : (1 - t) * 2 * 255));
  const b = Math.min(255, Math.round(t < 0.5 ? 255 - t * 2 * 255 : 0));
  return `rgba(${r},${g},${b},0.75)`;
}

interface HeatmapCanvasProps {
  cells: HeatCell[];
  cellPx: number;
  opacity: number;
}
function HeatmapCanvas({ cells, cellPx, opacity }: HeatmapCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (cells.length === 0) return;

    const max = Math.max(...cells.map(c => c.count));
    for (const { gx, gy, count } of cells) {
      ctx.globalAlpha = opacity;
      ctx.fillStyle = heatColor(count / max);
      ctx.fillRect(gx * cellPx, gy * cellPx, cellPx, cellPx);
    }
    ctx.globalAlpha = 1;
  }, [cells, cellPx, opacity]);

  const maxGx = cells.length ? Math.max(...cells.map(c => c.gx)) + 1 : 40;
  const maxGy = cells.length ? Math.max(...cells.map(c => c.gy)) + 1 : 30;
  const W = maxGx * cellPx;
  const H = maxGy * cellPx;

  return (
    <div className="relative bg-game-dark border border-game-border rounded overflow-auto">
      {cells.length === 0
        ? <div className="flex items-center justify-center h-64 text-game-textDim font-mono text-sm">NO DATA — kills will appear here once games are played</div>
        : <canvas ref={canvasRef} width={W} height={H} style={{ display: 'block', imageRendering: 'pixelated' }} />}
    </div>
  );
}

function HorizontalBar({ label, value, max, color, suffix = '' }: { label: string; value: number; max: number; color: string; suffix?: string }) {
  const pct = max > 0 ? (value / max) * 100 : 0;
  return (
    <div className="flex items-center gap-3 mb-2">
      <span className="w-24 text-xs font-mono text-game-textDim text-right shrink-0">{label}</span>
      <div className="flex-1 h-5 bg-game-dark rounded overflow-hidden border border-game-border">
        <div className="h-full rounded transition-all" style={{ width: `${pct}%`, backgroundColor: color }} />
      </div>
      <span className="w-16 text-xs font-mono text-game-text text-right shrink-0">{value.toLocaleString()}{suffix}</span>
    </div>
  );
}

function WeaponsPanel({ data }: { data: WeaponRow[] }) {
  const maxKills = Math.max(...data.map(w => w.kills), 1);
  return (
    <div className="space-y-6">
      <div>
        <div className="text-xs font-mono text-game-textDim mb-3 tracking-widest">KILLS BY WEAPON</div>
        {data.map((w, i) => (
          <HorizontalBar key={w.name} label={w.name} value={w.kills} max={maxKills} color={WEAPON_COLORS[i] ?? '#64748b'} />
        ))}
      </div>
      <table className="w-full text-xs font-mono border-collapse">
        <thead>
          <tr className="border-b border-game-border text-game-textDim">
            <th className="text-left py-2">Weapon</th>
            <th className="text-right py-2">Kills</th>
            <th className="text-right py-2">Fires</th>
            <th className="text-right py-2">Hits</th>
            <th className="text-right py-2">Accuracy</th>
          </tr>
        </thead>
        <tbody>
          {data.map((w, i) => (
            <tr key={w.name} className="border-b border-game-border/40 hover:bg-game-bgHover">
              <td className="py-2" style={{ color: WEAPON_COLORS[i] ?? '#94a3b8' }}>{w.name}</td>
              <td className="text-right py-2 text-game-text">{w.kills.toLocaleString()}</td>
              <td className="text-right py-2 text-game-textDim">{w.fires.toLocaleString()}</td>
              <td className="text-right py-2 text-game-textDim">{w.hits.toLocaleString()}</td>
              <td className="text-right py-2 text-game-textDim">{w.accuracy}%</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function AgenciesPanel({ data }: { data: AgencyRow[] }) {
  const maxWins = Math.max(...data.map(a => a.wins), 1);
  return (
    <div className="space-y-6">
      <div>
        <div className="text-xs font-mono text-game-textDim mb-3 tracking-widest">WINS BY AGENCY</div>
        {data.map(a => (
          <HorizontalBar key={a.name} label={a.name} value={a.wins} max={maxWins} color={AGENCY_COLORS[a.agencyIdx] ?? '#64748b'} />
        ))}
      </div>
      <div>
        <div className="text-xs font-mono text-game-textDim mb-3 tracking-widest">WIN RATE</div>
        {data.map(a => (
          <HorizontalBar key={a.name} label={a.name} value={a.winRate} max={100} color={AGENCY_COLORS[a.agencyIdx] ?? '#64748b'} suffix="%" />
        ))}
      </div>
      <table className="w-full text-xs font-mono border-collapse">
        <thead>
          <tr className="border-b border-game-border text-game-textDim">
            <th className="text-left py-2">Agency</th>
            <th className="text-right py-2">Wins</th>
            <th className="text-right py-2">Losses</th>
            <th className="text-right py-2">Win Rate</th>
          </tr>
        </thead>
        <tbody>
          {data.map(a => (
            <tr key={a.name} className="border-b border-game-border/40 hover:bg-game-bgHover">
              <td className="py-2 font-bold" style={{ color: AGENCY_COLORS[a.agencyIdx] ?? '#94a3b8' }}>{a.name}</td>
              <td className="text-right py-2 text-game-text">{a.wins.toLocaleString()}</td>
              <td className="text-right py-2 text-game-textDim">{a.losses.toLocaleString()}</td>
              <td className="text-right py-2 text-game-textDim">{a.winRate}%</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export default function Analytics() {
  useAuth();

  const [maps, setMaps]               = useState<{ mapName: string; kills: number }[]>([]);
  const [selectedMap, setSelectedMap] = useState('');
  const [tab, setTab]                 = useState<Tab>('heatmap');
  const [dateFrom, setDateFrom]       = useState('');
  const [dateTo, setDateTo]           = useState('');
  const [agency, setAgency]           = useState('');
  const [cellPx, setCellPx]           = useState(16);
  const [opacity, setOpacity]         = useState(0.75);

  const [heatData, setHeatData]       = useState<{ cells: HeatCell[]; cellPx: number } | null>(null);
  const [weaponsData, setWeaponsData] = useState<WeaponRow[] | null>(null);
  const [agenciesData, setAgenciesData] = useState<AgencyRow[] | null>(null);
  const [loading, setLoading]         = useState(false);
  const [error, setError]             = useState<string | null>(null);

  useEffect(() => {
    getAnalyticsMaps()
      .then(res => {
        setMaps(res.maps);
        if (res.maps.length > 0) setSelectedMap(res.maps[0].mapName);
      })
      .catch(e => setError(e.message));
  }, []);

  const filterParams = useCallback(() => {
    const p: Record<string, string> = {};
    if (dateFrom) p.dateFrom = dateFrom;
    if (dateTo)   p.dateTo   = dateTo;
    if (agency)   p.agency   = agency;
    return p;
  }, [dateFrom, dateTo, agency]);

  const load = useCallback(async () => {
    if (!selectedMap) return;
    setLoading(true);
    setError(null);
    try {
      if (tab === 'heatmap') {
        const params = { ...filterParams(), cellPx: String(cellPx) };
        const res = await getAnalyticsHeatmap(selectedMap, params) as { cells: HeatCell[]; cellPx: number };
        setHeatData(res);
      } else if (tab === 'weapons') {
        const res = await getAnalyticsWeapons(selectedMap, filterParams()) as { weapons: WeaponRow[] };
        setWeaponsData(res.weapons);
      } else {
        const res = await getAnalyticsAgencies(selectedMap, filterParams()) as { agencies: AgencyRow[] };
        setAgenciesData(res.agencies);
      }
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setLoading(false);
    }
  }, [selectedMap, tab, cellPx, filterParams]);

  useEffect(() => { load(); }, [load]);

  const TABS: { id: Tab; label: string }[] = [
    { id: 'heatmap',   label: '[ KILL HEATMAP ]' },
    { id: 'weapons',   label: '[ WEAPONS ]' },
    { id: 'agencies',  label: '[ AGENCIES ]' },
  ];

  return (
    <div className="flex h-screen bg-game-bg text-game-text overflow-hidden">
      <Sidebar />
      <main className="flex-1 flex flex-col overflow-hidden">
        {/* Header */}
        <div className="px-6 py-4 border-b border-game-border flex-shrink-0">
          <h1 className="text-lg font-mono font-bold tracking-widest text-game-primary">MATCH ANALYTICS</h1>
        </div>

        {/* Filters */}
        <div className="px-6 py-3 border-b border-game-border bg-game-bgCard flex-shrink-0 flex flex-wrap gap-4 items-end">
          <div>
            <div className="text-xs text-game-textDim font-mono mb-1">MAP</div>
            <select value={selectedMap} onChange={e => setSelectedMap(e.target.value)}
              className="bg-game-dark border border-game-border text-game-text text-xs font-mono px-2 py-1.5 rounded min-w-40">
              {maps.length === 0 && <option value="">No data yet</option>}
              {maps.map(m => (
                <option key={m.mapName} value={m.mapName}>{m.mapName} ({m.kills.toLocaleString()} kills)</option>
              ))}
            </select>
          </div>
          <div>
            <div className="text-xs text-game-textDim font-mono mb-1">AGENCY</div>
            <select value={agency} onChange={e => setAgency(e.target.value)}
              className="bg-game-dark border border-game-border text-game-text text-xs font-mono px-2 py-1.5 rounded">
              <option value="">All</option>
              {AGENCY_NAMES.map((n, i) => <option key={i} value={i}>{n}</option>)}
            </select>
          </div>
          <div>
            <div className="text-xs text-game-textDim font-mono mb-1">DATE FROM</div>
            <input type="date" value={dateFrom} onChange={e => setDateFrom(e.target.value)}
              className="bg-game-dark border border-game-border text-game-text text-xs font-mono px-2 py-1.5 rounded" />
          </div>
          <div>
            <div className="text-xs text-game-textDim font-mono mb-1">DATE TO</div>
            <input type="date" value={dateTo} onChange={e => setDateTo(e.target.value)}
              className="bg-game-dark border border-game-border text-game-text text-xs font-mono px-2 py-1.5 rounded" />
          </div>
          {tab === 'heatmap' && (
            <>
              <div>
                <div className="text-xs text-game-textDim font-mono mb-1">CELL SIZE</div>
                <select value={cellPx} onChange={e => setCellPx(Number(e.target.value))}
                  className="bg-game-dark border border-game-border text-game-text text-xs font-mono px-2 py-1.5 rounded">
                  {[8, 16, 32, 64].map(v => <option key={v} value={v}>{v}px</option>)}
                </select>
              </div>
              <div>
                <div className="text-xs text-game-textDim font-mono mb-1">OPACITY {Math.round(opacity * 100)}%</div>
                <input type="range" min={0.2} max={1} step={0.05} value={opacity}
                  onChange={e => setOpacity(Number(e.target.value))}
                  className="w-24 accent-game-primary" />
              </div>
            </>
          )}
          <button onClick={load}
            className="px-3 py-1.5 text-xs font-mono border border-game-primary text-game-primary hover:bg-game-primary hover:text-black rounded transition-colors">
            {loading ? '...' : 'APPLY'}
          </button>
        </div>

        {/* Tabs */}
        <div className="px-6 border-b border-game-border bg-game-bgCard flex-shrink-0 flex gap-1 pt-2">
          {TABS.map(t => (
            <button key={t.id} onClick={() => setTab(t.id)}
              className={`px-4 py-2 text-xs font-mono tracking-wide border-b-2 transition-colors
                ${tab === t.id
                  ? 'border-game-primary text-game-primary'
                  : 'border-transparent text-game-textDim hover:text-game-text'}`}>
              {t.label}
            </button>
          ))}
        </div>

        {/* Content */}
        <div className="flex-1 overflow-auto p-6">
          {error && (
            <div className="mb-4 px-4 py-3 bg-game-danger/10 border border-game-danger text-game-danger text-xs font-mono rounded">
              {error}
            </div>
          )}
          {tab === 'heatmap' && (
            <div className="space-y-3">
              <div className="flex items-center gap-4 text-xs font-mono text-game-textDim">
                <span>COOL</span>
                <div className="flex h-3 w-48 rounded overflow-hidden">
                  {Array.from({ length: 20 }, (_, i) => (
                    <div key={i} className="flex-1" style={{ backgroundColor: heatColor(i / 19) }} />
                  ))}
                </div>
                <span>HOT</span>
                {heatData && <span className="ml-4">{heatData.cells.length} cells · {heatData.cells.reduce((s, c) => s + c.count, 0).toLocaleString()} kills</span>}
              </div>
              <HeatmapCanvas cells={heatData?.cells ?? []} cellPx={heatData?.cellPx ?? cellPx} opacity={opacity} />
            </div>
          )}
          {tab === 'weapons' && (
            weaponsData && weaponsData.length > 0
              ? <WeaponsPanel data={weaponsData} />
              : <div className="text-game-textDim font-mono text-sm">No weapon data for this map.</div>
          )}
          {tab === 'agencies' && (
            agenciesData && agenciesData.length > 0
              ? <AgenciesPanel data={agenciesData} />
              : <div className="text-game-textDim font-mono text-sm">No agency data for this map.</div>
          )}
        </div>
      </main>
    </div>
  );
}
