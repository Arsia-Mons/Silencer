'use client';
import { useRef, useState, useCallback, useEffect } from 'react';
import Link from 'next/link';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { useWsConnected } from '../../lib/socket';

// ── Types ────────────────────────────────────────────────────────────────────

interface WeaponDef {
  id: string;
  projectileType?: string;
  // Numeric ballistics (owned by GAS editor — read-only here)
  healthDamage?: number;
  shieldDamage?: number;
  fireDelay?: number;
  velocity?: number;
  moveAmount?: number;
  radius?: number;
  // Sprite banks (8-directional: up, upR, right, downR, down, downL, left, upL)
  spriteBanks?: number[];
  hitOverlayBank?: number;
  // Sounds
  soundFire?: string;
  soundHit1?: string;
  soundHit2?: string;
  soundLoop?: string;
  soundExplosion?: string;
  soundLand?: string;
  soundThrow?: string;
  // Ammo
  ammoCapacity?: number;
  reloadTicks?: number;
  // Allow additional numeric fields from GAS
  [key: string]: unknown;
}

interface AgencyDef {
  id: number;
  name: string;
  weapons?: string[];
  [key: string]: unknown;
}

interface FolderState {
  weapons: WeaponDef[];
  agencies: AgencyDef[];
  weaponsFile: FileSystemFileHandle | null;
  agenciesFile: FileSystemFileHandle | null;
  folderName: string;
}

const DIRECTIONS = ['up', 'upR', 'right', 'downR', 'down', 'downL', 'left', 'upL'] as const;

const SOUND_FIELDS: { key: keyof WeaponDef; label: string }[] = [
  { key: 'soundFire',      label: 'Fire' },
  { key: 'soundHit1',      label: 'Hit 1' },
  { key: 'soundHit2',      label: 'Hit 2' },
  { key: 'soundLoop',      label: 'Loop' },
  { key: 'soundExplosion', label: 'Explosion' },
  { key: 'soundLand',      label: 'Land' },
  { key: 'soundThrow',     label: 'Throw' },
];

const NUMERIC_BALLISTICS: { key: string; label: string }[] = [
  { key: 'healthDamage',   label: 'Health DMG' },
  { key: 'shieldDamage',   label: 'Shield DMG' },
  { key: 'fireDelay',      label: 'Fire Delay (ticks)' },
  { key: 'velocity',       label: 'Velocity (px/tick)' },
  { key: 'moveAmount',     label: 'Move Steps/tick' },
  { key: 'radius',         label: 'Blast Radius (px)' },
  { key: 'ammoCapacity',   label: 'Ammo Capacity' },
  { key: 'reloadTicks',    label: 'Reload Ticks' },
];

// ── Helpers ──────────────────────────────────────────────────────────────────

async function readJson(handle: FileSystemFileHandle): Promise<unknown> {
  const file = await handle.getFile();
  return JSON.parse(await file.text());
}

async function writeJson(handle: FileSystemFileHandle, data: unknown): Promise<void> {
  const writable = await handle.createWritable();
  await writable.write(JSON.stringify(data, null, 2));
  await writable.close();
}

// ── Component ────────────────────────────────────────────────────────────────

export default function WeaponsPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const [folder, setFolder] = useState<FolderState | null>(null);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');

  // ── Open folder ─────────────────────────────────────────────────────────
  const openFolder = useCallback(async () => {
    try {
      const dirHandle = await (window as unknown as { showDirectoryPicker(): Promise<FileSystemDirectoryHandle> }).showDirectoryPicker();
      let weaponsFile: FileSystemFileHandle | null = null;
      let agenciesFile: FileSystemFileHandle | null = null;
      for await (const [name, handle] of (dirHandle as unknown as AsyncIterable<[string, FileSystemHandle]>)) {
        if (name === 'weapons.json') weaponsFile = handle as FileSystemFileHandle;
        if (name === 'agencies.json') agenciesFile = handle as FileSystemFileHandle;
      }
      if (!weaponsFile) { setError('weapons.json not found in selected folder.'); return; }
      if (!agenciesFile) { setError('agencies.json not found in selected folder.'); return; }
      const [wData, aData] = await Promise.all([readJson(weaponsFile), readJson(agenciesFile)]);
      const weapons = ((wData as Record<string, unknown>).weapons as WeaponDef[]) ?? [];
      const agencies = ((aData as Record<string, unknown>).agencies as AgencyDef[]) ?? [];
      setFolder({ weapons, agencies, weaponsFile, agenciesFile, folderName: dirHandle.name });
      setSelectedId(weapons[0]?.id ?? null);
      setDirty(false);
      setError('');
    } catch (e) {
      if ((e as Error).name !== 'AbortError') setError(String(e));
    }
  }, []);

  // ── Close folder ────────────────────────────────────────────────────────
  function closeFolder() {
    setFolder(null);
    setSelectedId(null);
    setDirty(false);
  }

  // ── Patch weapon field ───────────────────────────────────────────────────
  function patchWeapon(id: string, patch: Partial<WeaponDef>) {
    setFolder(f => {
      if (!f) return f;
      return { ...f, weapons: f.weapons.map(w => w.id === id ? { ...w, ...patch } : w) };
    });
    setDirty(true);
  }

  // ── Patch sprite bank at direction index ─────────────────────────────────
  function patchSpriteBank(id: string, dirIdx: number, value: number) {
    setFolder(f => {
      if (!f) return f;
      return {
        ...f,
        weapons: f.weapons.map(w => {
          if (w.id !== id) return w;
          const banks = [...(w.spriteBanks ?? [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF])];
          while (banks.length < 8) banks.push(0xFF);
          banks[dirIdx] = value;
          return { ...w, spriteBanks: banks };
        }),
      };
    });
    setDirty(true);
  }

  // ── Toggle agency weapon ─────────────────────────────────────────────────
  function toggleAgencyWeapon(agencyId: number, weaponId: string) {
    setFolder(f => {
      if (!f) return f;
      return {
        ...f,
        agencies: f.agencies.map(a => {
          if (a.id !== agencyId) return a;
          const has = (a.weapons ?? []).includes(weaponId);
          const weapons = has
            ? (a.weapons ?? []).filter(w => w !== weaponId)
            : [...(a.weapons ?? []), weaponId];
          return { ...a, weapons };
        }),
      };
    });
    setDirty(true);
  }

  // ── Save ─────────────────────────────────────────────────────────────────
  async function handleSave() {
    if (!folder) return;
    setSaving(true);
    try {
      // Reconstruct full JSON preserving top-level structure
      const [wRaw, aRaw] = await Promise.all([
        readJson(folder.weaponsFile!),
        readJson(folder.agenciesFile!),
      ]);
      const wOut = { ...(wRaw as Record<string, unknown>), weapons: folder.weapons };
      const aOut = { ...(aRaw as Record<string, unknown>), agencies: folder.agencies };
      await Promise.all([
        writeJson(folder.weaponsFile!, wOut),
        writeJson(folder.agenciesFile!, aOut),
      ]);
      setDirty(false);
    } catch (e) {
      setError(String(e));
    } finally {
      setSaving(false);
    }
  }

  const currentWeapon = folder?.weapons.find(w => w.id === selectedId) ?? null;

  // ── Empty state ──────────────────────────────────────────────────────────
  if (!folder) {
    return (
      <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
        <Sidebar wsConnected={wsConnected} />
        <div className="flex-1 flex items-center justify-center">
          <div className="flex flex-col items-center gap-6 text-center max-w-sm">
            <div className="text-5xl">⚔</div>
            <div className="text-xl font-mono tracking-widest text-[#00a328]">WEAPON TOOL</div>
            <div className="text-xs text-[#4a7a4a] font-mono leading-relaxed">
              Visual weapon authoring tool. Open your <code className="text-[#7aaa7a]">shared/assets/gas/</code> folder
              to manage sprite bank assignments, sounds, and agency loadouts.
            </div>
            <ul className="text-[10px] text-[#4a7a4a] font-mono text-left space-y-1">
              <li>◆ Sprite bank pickers (8-directional)</li>
              <li>◆ Sound event assignments</li>
              <li>◆ Agency weapon loadouts</li>
              <li>◆ Numeric stats (read-only — edit in GAS)</li>
              <li>◆ Ballistics trajectory preview</li>
            </ul>
            <button
              onClick={openFolder}
              className="px-8 py-3 border border-[#00a328] text-[#00a328] font-mono text-sm tracking-widest hover:bg-[#00a328]/10 transition-colors"
            >
              [ OPEN FOLDER ]
            </button>
          </div>
        </div>
      </div>
    );
  }

  // ── Main layout ──────────────────────────────────────────────────────────
  return (
    <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
      <Sidebar wsConnected={wsConnected} />

      {/* Header */}
      <div className="flex flex-col flex-1 min-w-0">
        <div className="flex items-center gap-3 px-4 py-2 border-b border-[#1a2e1a] shrink-0">
          <span className="text-xs font-mono text-[#00a328] tracking-widest">⚔ WEAPON TOOL</span>
          <span className="text-xs font-mono text-[#4a7a4a]">[ {folder.folderName} ]</span>
          <div className="flex-1" />
          {error && <span className="text-xs text-red-400 font-mono">{error}</span>}
          {dirty && (
            <button
              onClick={handleSave}
              disabled={saving}
              className="px-3 py-1 text-xs font-mono border border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10 disabled:opacity-50 transition-colors"
            >
              {saving ? 'SAVING…' : '↓ SAVE'}
            </button>
          )}
          {dirty && <span className="text-[10px] font-mono text-[#f59e0b]">● unsaved</span>}
          <button
            onClick={closeFolder}
            className="px-2 py-1 text-xs font-mono text-[#4a7a4a] hover:text-red-400 border border-[#1a2e1a] hover:border-red-400 transition-colors"
          >
            ✕
          </button>
        </div>

        <div className="flex flex-1 min-h-0">
          {/* Left: weapon list */}
          <div className="flex flex-col border-r border-[#1a2e1a]" style={{ width: 180, minWidth: 180 }}>
            <div className="px-3 py-2 border-b border-[#1a2e1a]">
              <span className="text-[10px] font-mono text-[#4a7a4a]">WEAPONS</span>
            </div>
            <div className="flex-1 overflow-y-auto">
              {folder.weapons.map(w => (
                <button
                  key={w.id}
                  onClick={() => setSelectedId(w.id)}
                  className={`w-full text-left px-3 py-2 text-xs font-mono border-b border-[#0d1f0d] transition-colors ${
                    w.id === selectedId
                      ? 'bg-[#00a328]/10 text-[#00a328] border-l-2 border-l-[#00a328]'
                      : 'text-[#7aaa7a] hover:text-[#d1fad7] hover:bg-[#1a2e1a]'
                  }`}
                >
                  <div className="tracking-wider uppercase">{w.id}</div>
                  {w.projectileType && (
                    <div className="text-[9px] text-[#4a7a4a] mt-0.5">{w.projectileType}</div>
                  )}
                </button>
              ))}
            </div>
          </div>

          {/* Center + Right: weapon detail */}
          {currentWeapon ? (
            <div className="flex-1 overflow-y-auto p-5 flex flex-col gap-6">
              <div className="flex items-center gap-3">
                <h2 className="text-lg font-mono tracking-widest text-[#00a328] uppercase">{currentWeapon.id}</h2>
                {currentWeapon.projectileType && (
                  <span className="text-xs font-mono text-[#4a7a4a] border border-[#1a2e1a] px-2 py-0.5">
                    {currentWeapon.projectileType}
                  </span>
                )}
              </div>

              <div className="grid grid-cols-1 lg:grid-cols-2 xl:grid-cols-3 gap-5">
                {/* Numeric ballistics (read-only) */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <div className="flex items-center justify-between">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">BALLISTICS</span>
                    <Link
                      href="/gas?tab=weapons"
                      className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors"
                    >
                      EDIT IN GAS →
                    </Link>
                  </div>
                  {NUMERIC_BALLISTICS.map(({ key, label }) =>
                    currentWeapon[key] !== undefined && currentWeapon[key] !== 0 ? (
                      <div key={key} className="flex justify-between items-center">
                        <span className="text-[10px] font-mono text-[#7aaa7a]">{label}</span>
                        <span className="text-xs font-mono text-[#d1fad7]">{String(currentWeapon[key])}</span>
                      </div>
                    ) : null
                  )}
                </section>

                {/* Sprite banks */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <div className="flex items-center justify-between">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SPRITE BANKS</span>
                    <Link
                      href="/sprites"
                      className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors"
                    >
                      BROWSE →
                    </Link>
                  </div>
                  <div className="grid grid-cols-2 gap-2">
                    {DIRECTIONS.map((dir, i) => {
                      const banks = currentWeapon.spriteBanks ?? [];
                      const val = banks[i] ?? 0xFF;
                      return (
                        <div key={dir} className="flex items-center gap-2">
                          <span className="text-[9px] font-mono text-[#4a7a4a] w-8">{dir}</span>
                          <input
                            type="number"
                            min={0}
                            max={255}
                            value={val === 0xFF ? '' : val}
                            placeholder="0xFF"
                            onChange={e => {
                              const n = e.target.value === '' ? 0xFF : parseInt(e.target.value, 10);
                              patchSpriteBank(currentWeapon.id, i, isNaN(n) ? 0xFF : n);
                            }}
                            className="w-full bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded focus:border-[#00a328] outline-none"
                          />
                        </div>
                      );
                    })}
                  </div>
                  <div className="flex items-center gap-2 border-t border-[#1a2e1a] pt-3">
                    <span className="text-[9px] font-mono text-[#4a7a4a] w-20">hit overlay</span>
                    <input
                      type="number"
                      min={-1}
                      max={255}
                      value={currentWeapon.hitOverlayBank ?? -1}
                      onChange={e => patchWeapon(currentWeapon.id, { hitOverlayBank: parseInt(e.target.value, 10) || -1 })}
                      className="w-full bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded focus:border-[#00a328] outline-none"
                    />
                  </div>
                </section>

                {/* Sounds */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <div className="flex items-center justify-between">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SOUNDS</span>
                    <Link
                      href="/sound-studio"
                      className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors"
                    >
                      BROWSE →
                    </Link>
                  </div>
                  {SOUND_FIELDS.map(({ key, label }) => (
                    <div key={String(key)} className="flex items-center gap-2">
                      <span className="text-[9px] font-mono text-[#4a7a4a] w-16">{label}</span>
                      <input
                        type="text"
                        value={String(currentWeapon[key] ?? '')}
                        placeholder="filename.wav"
                        onChange={e => patchWeapon(currentWeapon.id, { [key]: e.target.value })}
                        className="flex-1 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded focus:border-[#00a328] outline-none"
                      />
                    </div>
                  ))}
                </section>

                {/* Agency loadouts */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">AGENCY LOADOUT</span>
                  <p className="text-[9px] text-[#4a7a4a] font-mono">Which agencies can equip this weapon.</p>
                  {folder.agencies.map(agency => {
                    const has = (agency.weapons ?? []).includes(currentWeapon.id);
                    return (
                      <label key={agency.id} className="flex items-center gap-3 cursor-pointer group">
                        <div
                          onClick={() => toggleAgencyWeapon(agency.id, currentWeapon.id)}
                          className={`w-3 h-3 border flex items-center justify-center shrink-0 transition-colors cursor-pointer ${
                            has ? 'border-[#00a328] bg-[#00a328]' : 'border-[#1a2e1a] group-hover:border-[#4a7a4a]'
                          }`}
                        >
                          {has && <span className="text-[8px] text-black">✓</span>}
                        </div>
                        <span className="text-xs font-mono text-[#7aaa7a] group-hover:text-[#d1fad7] transition-colors">
                          {agency.name ?? `Agency ${agency.id}`}
                        </span>
                      </label>
                    );
                  })}
                </section>

                {/* Ballistics preview — placeholder for Phase 6 */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3 lg:col-span-2">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">BALLISTICS PREVIEW</span>
                  <BallisticsPreview weapon={currentWeapon} />
                </section>
              </div>
            </div>
          ) : (
            <div className="flex-1 flex items-center justify-center text-[#2a4a2a] text-xs font-mono">
              Select a weapon
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

// ── Ballistics preview canvas ────────────────────────────────────────────────

function BallisticsPreview({ weapon }: { weapon: WeaponDef }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const simulate = useCallback(() => {
    const cvs = canvasRef.current;
    if (!cvs) return;
    const ctx = cvs.getContext('2d');
    if (!ctx) return;

    const W = cvs.width, H = cvs.height;
    const velocity = weapon.velocity ?? 0;
    const gravity = (weapon as Record<string, unknown>).plasmaGravity as number ?? 0;
    const moveAmount = weapon.moveAmount ?? 1;
    const radius = weapon.radius ?? 0;
    const type = weapon.projectileType ?? 'physics';

    ctx.fillStyle = '#080f08';
    ctx.fillRect(0, 0, W, H);

    // Ground line
    ctx.strokeStyle = '#1a2e1a';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, H - 20); ctx.lineTo(W, H - 20); ctx.stroke();

    if (velocity === 0 && type !== 'grenade') {
      ctx.fillStyle = '#2a4a2a';
      ctx.font = '10px monospace';
      ctx.fillText('no velocity — melee / instant', 10, H / 2);
      return;
    }

    // Simulate trajectory
    const scale = 1.5;
    let x = 20, y = H - 20, xv = velocity * scale, yv = 0;
    const path: [number, number][] = [[x, y]];

    if (type === 'grenade' || type === 'arcing') {
      yv = -8 * scale;
      xv = (weapon.throwSpeedStanding as number ?? velocity ?? 20) * scale * 0.5;
    }

    const maxSteps = 600;
    for (let step = 0; step < maxSteps; step++) {
      for (let m = 0; m < Math.max(1, moveAmount); m++) {
        x += xv / Math.max(1, moveAmount);
        y += yv / Math.max(1, moveAmount);
      }
      if (gravity) yv += gravity * 0.3;
      if (y >= H - 20 && (type === 'grenade' || type === 'arcing')) break;
      if (y >= H - 20) break;
      if (x > W) break;
      path.push([x, y]);
    }

    // Draw path
    ctx.strokeStyle = '#00a32888';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    path.forEach(([px, py], i) => i === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py));
    ctx.stroke();

    // Draw impact point
    const [ix, iy] = path[path.length - 1];
    ctx.fillStyle = '#00a328';
    ctx.beginPath(); ctx.arc(ix, iy, 3, 0, Math.PI * 2); ctx.fill();

    // Draw splash radius
    if (radius > 0) {
      ctx.strokeStyle = '#f59e0b44';
      ctx.lineWidth = 1;
      ctx.beginPath(); ctx.arc(ix, iy, radius * scale * 0.5, 0, Math.PI * 2); ctx.stroke();
      ctx.fillStyle = '#f59e0b22';
      ctx.beginPath(); ctx.arc(ix, iy, radius * scale * 0.5, 0, Math.PI * 2); ctx.fill();
    }

    // Labels
    ctx.fillStyle = '#4a7a4a';
    ctx.font = '9px monospace';
    ctx.fillText(`v=${velocity} mv=${moveAmount}${gravity ? ` g=${gravity}` : ''}${radius ? ` r=${radius}px` : ''}`, 6, 12);
  }, [weapon]);

  // Draw whenever weapon changes
  useEffect(() => { simulate(); }, [simulate]);

  return (
    <div className="flex flex-col gap-2">
      <canvas
        ref={canvasRef}
        width={560}
        height={160}
        className="w-full border border-[#1a2e1a] rounded bg-[#080f08]"
        style={{ imageRendering: 'pixelated' }}
      />
      <div className="flex gap-2">
        <button
          onClick={simulate}
          className="px-3 py-1 text-[10px] font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:border-[#00a328] hover:text-[#00a328] transition-colors"
        >
          ▶ REDRAW
        </button>
        <span className="text-[9px] font-mono text-[#2a4a2a] self-center">tick-accurate simulation</span>
      </div>
    </div>
  );
}
