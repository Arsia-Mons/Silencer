'use client';
import { useRef, useState, useCallback, useEffect } from 'react';
import Link from 'next/link';
import { useAuth } from '../../lib/auth';
import Sidebar from '../../components/Sidebar';
import { useWsConnected } from '../../lib/socket';
import { decodeAdpcmWav } from '../sound-studio/adpcm';
import { apiFetch } from '../../lib/api';
import * as gasStore from '../../lib/gas-store';
import * as audioStore from '../../lib/audio-store';
import * as soundStudioStore from '../../lib/sound-studio-store';

// ── Types ────────────────────────────────────────────────────────────────────

interface WeaponDef {
  id: string;
  projectileType?: string;
  healthDamage?: number;
  shieldDamage?: number;
  healthDamageLarge?: number;
  shieldDamageLarge?: number;
  fireDelay?: number;
  velocity?: number;
  moveAmount?: number;
  radius?: number;
  spriteBanks?: number[];
  hitOverlayBank?: number;
  soundFire?: string;
  soundHit1?: string;
  soundHit2?: string;
  soundLoop?: string;
  soundExplosion?: string;
  soundLand?: string;
  soundThrow?: string;
  soundWarn?: string;
  ammoCapacity?: number;
  reloadTicks?: number;
  // Grenade timing + throw speeds
  throwSpeedStanding?: number;
  throwSpeedMoving?: number;
  throwSpeedRunning?: number;
  explosionTick?: number;
  secondaryTick?: number;
  destroyTick?: number;
  neutronDestroyTick?: number;
  flareDuration?: number;
  neutronTraceTime?: number;
  detonatorLaunchYv?: number;
  // Rocket physics
  rocketSlowInitial?: number;
  rocketHoverTick?: number;
  rocketSlowHover?: number;
  // Plasma physics
  plasmaGravity?: number;
  plasmaLifeNormal?: number;
  plasmaLifeLarge?: number;
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
  rawWeapons: Record<string, unknown>;
  rawAgencies: Record<string, unknown>;
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
  { key: 'soundWarn',      label: 'Warn' },
];

// Sound fields that are relevant per projectile type.
// Fields with an existing value are always shown regardless of type.
const SOUNDS_BY_TYPE: Partial<Record<string, (keyof WeaponDef)[]>> = {
  physics: ['soundFire', 'soundHit1', 'soundHit2'],
  wall:    ['soundFire', 'soundHit1', 'soundHit2'],
  rocket:  ['soundLoop', 'soundExplosion', 'soundLand'],
  flamer:  ['soundLoop'],
  grenade: ['soundThrow', 'soundExplosion', 'soundLand', 'soundHit1', 'soundWarn'],
  arcing:  [],
  plasma:  [],
};

function soundFieldsForWeapon(weapon: WeaponDef): { key: keyof WeaponDef; label: string }[] {
  const relevant = SOUNDS_BY_TYPE[weapon.projectileType ?? ''] ?? SOUND_FIELDS.map(f => f.key);
  // Always include fields that already carry a value.
  const withValue = SOUND_FIELDS.filter(f => weapon[f.key]).map(f => f.key);
  const keys = Array.from(new Set([...relevant, ...withValue]));
  return SOUND_FIELDS.filter(f => keys.includes(f.key));
}

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

const GRENADE_FIELDS: { key: string; label: string }[] = [
  { key: 'throwSpeedStanding',  label: 'Throw Speed (standing)' },
  { key: 'throwSpeedMoving',    label: 'Throw Speed (moving)' },
  { key: 'throwSpeedRunning',   label: 'Throw Speed (running)' },
  { key: 'explosionTick',       label: 'Explosion Tick' },
  { key: 'secondaryTick',       label: 'Secondary Tick' },
  { key: 'destroyTick',         label: 'Destroy Tick' },
  { key: 'neutronDestroyTick',  label: 'Neutron Destroy Tick' },
  { key: 'flareDuration',       label: 'Flare Duration (ticks)' },
  { key: 'neutronTraceTime',    label: 'Neutron Trace Time' },
  { key: 'detonatorLaunchYv',   label: 'Detonator Launch yv' },
];

const ROCKET_FIELDS: { key: string; label: string }[] = [
  { key: 'rocketSlowInitial', label: 'Launch Speed Mult' },
  { key: 'rocketHoverTick',   label: 'Hover Start Tick' },
  { key: 'rocketSlowHover',   label: 'Hover Speed Mult' },
];

const PLASMA_FIELDS: { key: string; label: string }[] = [
  { key: 'plasmaGravity',    label: 'Gravity (yv/tick)' },
  { key: 'plasmaLifeNormal', label: 'Life Normal (ticks)' },
  { key: 'plasmaLifeLarge',  label: 'Life Large (ticks)' },
  { key: 'healthDamageLarge', label: 'Health DMG (large)' },
  { key: 'shieldDamageLarge', label: 'Shield DMG (large)' },
];

// ── Component ────────────────────────────────────────────────────────────────

export default function WeaponsPage() {
  useAuth();
  const wsConnected = useWsConnected();
  const folderInputRef = useRef<HTMLInputElement>(null);
  const [folder, setFolder] = useState<FolderState | null>(null);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [dirty, setDirty] = useState(false);
  const [error, setError] = useState('');

  // ── Audio (ADPCM via Web Audio API, same as Sound Studio) ───────────────────
  const audioCtxRef = useRef<AudioContext | null>(null);
  const audioSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const [playingSound, setPlayingSound] = useState<string | null>(null);

  // ── Sound list for dropdowns ─────────────────────────────────────────────────
  const [soundList, setSoundList] = useState<string[]>(
    () => (soundStudioStore.get()?.sounds ?? []).map(s => s.name).sort(),
  );
  useEffect(() => {
    if (soundList.length > 0) return;
    apiFetch('/sounds').then((data: unknown) => {
      const names = (data as Array<{ name: string }>).map(s => s.name).sort();
      setSoundList(names);
    }).catch(() => {});
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const getToken = () => (typeof window !== 'undefined' ? localStorage.getItem('zs_token') ?? '' : '');

  function getAudioCtx(): AudioContext {
    if (!audioCtxRef.current || audioCtxRef.current.state === 'closed') {
      audioCtxRef.current = new AudioContext();
    }
    return audioCtxRef.current;
  }

  async function playSound(name: string) {
    if (audioSourceRef.current) {
      try { audioSourceRef.current.stop(); } catch { /* already ended */ }
      audioSourceRef.current = null;
    }
    if (playingSound === name) { setPlayingSound(null); return; }
    try {
      let decoded = audioStore.get(name);
      if (!decoded) {
        const r = await fetch(`/api/sounds/${encodeURIComponent(name)}/play`, {
          headers: { Authorization: `Bearer ${getToken()}` },
        });
        if (!r.ok) {
          const err = await r.json().catch(() => ({ error: r.statusText })) as { error?: string };
          throw new Error(err.error ?? r.statusText);
        }
        const buf = await r.arrayBuffer();
        const ctx = getAudioCtx();
        if (ctx.state === 'suspended') await ctx.resume();
        decoded = await decodeAdpcmWav(buf, ctx);
        audioStore.set(name, decoded);
      }
      const ctx = getAudioCtx();
      const source = ctx.createBufferSource();
      source.buffer = decoded;
      source.connect(ctx.destination);
      source.start();
      audioSourceRef.current = source;
      setPlayingSound(name);
      source.onended = () => { setPlayingSound(null); audioSourceRef.current = null; };
    } catch (e: unknown) {
      setPlayingSound(null);
      setError(`Cannot play ${name}: ${e instanceof Error ? e.message : String(e)}`);
    }
  }

  // ── Hydrate from shared GAS store on mount ───────────────────────────────
  useEffect(() => {
    if (gasStore.isLoaded() && !folder) {
      const wText = gasStore.getFile('weapons');
      const aText = gasStore.getFile('agencies');
      if (wText && aText) {
        try {
          const wData = JSON.parse(wText) as Record<string, unknown>;
          const aData = JSON.parse(aText) as Record<string, unknown>;
          const weapons = (wData.weapons as WeaponDef[]) ?? [];
          const agencies = (aData.agencies as AgencyDef[]) ?? [];
          setFolder({ weapons, agencies, rawWeapons: wData, rawAgencies: aData, folderName: gasStore.getFolderName() ?? 'gas' });
          setSelectedId(weapons[0]?.id ?? null);
        } catch { /* corrupt store data — ignore */ }
      }
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ── Keep GAS store in sync with edits ────────────────────────────────────
  useEffect(() => {
    if (!folder) return;
    gasStore.setFile('weapons',  JSON.stringify({ ...folder.rawWeapons,  weapons:  folder.weapons  }, null, 2));
    gasStore.setFile('agencies', JSON.stringify({ ...folder.rawAgencies, agencies: folder.agencies }, null, 2));
  }, [folder]);

  // ── Open folder (webkitdirectory) ────────────────────────────────────────
  async function handleFolderPicked(e: React.ChangeEvent<HTMLInputElement>) {
    // Snapshot into array BEFORE resetting the input (FileList is a live reference)
    const files = Array.from(e.target.files ?? []);
    e.target.value = '';
    if (files.length === 0) return;

    const folderName = files[0]?.webkitRelativePath?.split('/')[0] ?? 'gas';
    const wFile  = files.find(f => f.name === 'weapons.json');
    const aFile  = files.find(f => f.name === 'agencies.json');

    if (!wFile)  { setError('weapons.json not found in selected folder.'); return; }
    if (!aFile)  { setError('agencies.json not found in selected folder.'); return; }

    const [weaponsText, agenciesText] = await Promise.all([wFile.text(), aFile.text()]);

    try {
      const wData = JSON.parse(weaponsText) as Record<string, unknown>;
      const aData = JSON.parse(agenciesText) as Record<string, unknown>;
      const weapons = (wData.weapons as WeaponDef[]) ?? [];
      const agencies = (aData.agencies as AgencyDef[]) ?? [];
      setFolder({ weapons, agencies, rawWeapons: wData, rawAgencies: aData, folderName: folderName || 'gas' });
      setSelectedId(weapons[0]?.id ?? null);
      setDirty(false);
      setError('');
      gasStore.loadFolder(folderName || 'gas', { weapons: weaponsText, agencies: agenciesText });
    } catch (err) {
      setError(String(err));
    }
  }

  function closeFolder() {
    gasStore.clear();
    setFolder(null);
    setSelectedId(null);
    setDirty(false);
  }

  // ── Patch weapon ─────────────────────────────────────────────────────────
  function patchWeapon(id: string, patch: Partial<WeaponDef>) {
    setFolder(f => f ? { ...f, weapons: f.weapons.map(w => w.id === id ? { ...w, ...patch } : w) } : f);
    setDirty(true);
  }

  function patchSpriteBank(id: string, dirIdx: number, value: number) {
    setFolder(f => {
      if (!f) return f;
      return {
        ...f,
        weapons: f.weapons.map(w => {
          if (w.id !== id) return w;
          const banks = [...(w.spriteBanks ?? Array(8).fill(0xFF))];
          while (banks.length < 8) banks.push(0xFF);
          banks[dirIdx] = value;
          return { ...w, spriteBanks: banks };
        }),
      };
    });
    setDirty(true);
  }

  function toggleAgencyWeapon(agencyId: number, weaponId: string) {
    setFolder(f => {
      if (!f) return f;
      return {
        ...f,
        agencies: f.agencies.map(a => {
          if (a.id !== agencyId) return a;
          const has = (a.weapons ?? []).includes(weaponId);
          return { ...a, weapons: has ? (a.weapons ?? []).filter(w => w !== weaponId) : [...(a.weapons ?? []), weaponId] };
        }),
      };
    });
    setDirty(true);
  }

  // ── Download modified JSONs ───────────────────────────────────────────────
  function downloadJson(filename: string, data: unknown) {
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = filename; a.click();
    URL.revokeObjectURL(url);
  }

  function handleSave() {
    if (!folder) return;
    downloadJson('weapons.json',  { ...folder.rawWeapons,  weapons:  folder.weapons });
    downloadJson('agencies.json', { ...folder.rawAgencies, agencies: folder.agencies });
    setDirty(false);
  }

  const currentWeapon = folder?.weapons.find(w => w.id === selectedId) ?? null;

  // ── Empty state ──────────────────────────────────────────────────────────
  if (!folder) {
    return (
      <div className="flex h-screen overflow-hidden bg-[#080f08] text-[#d1fad7]">
        <Sidebar wsConnected={wsConnected} />
        <input
          ref={folderInputRef}
          type="file"
          // @ts-expect-error non-standard attribute
          webkitdirectory=""
          multiple
          className="hidden"
          onChange={handleFolderPicked}
        />
        <div className="flex-1 flex items-center justify-center">
          <div className="flex flex-col items-center gap-6 text-center max-w-sm">
            <div className="text-5xl">⚔</div>
            <div className="text-xl font-mono tracking-widest text-[#00a328]">WEAPON TOOL</div>
            <div className="text-xs text-[#4a7a4a] font-mono leading-relaxed">
              Open your <code className="text-[#7aaa7a]">shared/assets/gas/</code> folder to manage
              sprite bank assignments, sounds, and agency loadouts.
            </div>
            <ul className="text-[10px] text-[#4a7a4a] font-mono text-left space-y-1">
              <li>◆ Sprite bank pickers (8-directional)</li>
              <li>◆ Sound event assignments</li>
              <li>◆ Agency weapon loadouts</li>
              <li>◆ Numeric stats (read-only — edit in GAS)</li>
              <li>◆ Ballistics trajectory preview</li>
            </ul>
            {error && <p className="text-xs text-red-400 font-mono">{error}</p>}
            <button
              onClick={() => folderInputRef.current?.click()}
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
      <input
        ref={folderInputRef}
        type="file"
        // @ts-expect-error non-standard attribute
        webkitdirectory=""
        multiple
        className="hidden"
        onChange={handleFolderPicked}
      />

      <div className="flex flex-col flex-1 min-w-0">
        {/* Header */}
        <div className="flex items-center gap-3 px-4 py-2 border-b border-[#1a2e1a] shrink-0">
          <span className="text-xs font-mono text-[#00a328] tracking-widest">⚔ WEAPON TOOL</span>
          <span className="text-xs font-mono text-[#4a7a4a]">[ {folder.folderName} ]</span>
          <div className="flex-1" />
          {error && <span className="text-xs text-red-400 font-mono">{error}</span>}
          {dirty && (
            <>
              <span className="text-[10px] font-mono text-[#f59e0b]">● unsaved</span>
              <button
                onClick={handleSave}
                className="px-3 py-1 text-xs font-mono border border-[#00a328] text-[#00a328] hover:bg-[#00a328]/10 transition-colors"
              >
                ↓ DOWNLOAD JSON
              </button>
            </>
          )}
          <button
            onClick={closeFolder}
            className="px-2 py-1 text-xs font-mono text-[#4a7a4a] hover:text-red-400 border border-[#1a2e1a] hover:border-red-400 transition-colors"
          >
            ✕
          </button>
        </div>

        <div className="flex flex-1 min-h-0">
          {/* Weapon list */}
          <div className="flex flex-col border-r border-[#1a2e1a] h-full" style={{ width: 180, minWidth: 180 }}>
            <div className="px-3 py-2 border-b border-[#1a2e1a] shrink-0">
              <span className="text-[10px] font-mono text-[#4a7a4a]">WEAPONS</span>
            </div>
            <div className="flex-1 overflow-y-auto min-h-0">
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

          {/* Weapon detail */}
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
                {/* Ballistics (read-only) */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <div className="flex items-center justify-between">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">BALLISTICS</span>
                    <Link href="/gas?tab=weapons" className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors">
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

                {/* Grenade-specific fields */}
                {currentWeapon.projectileType === 'grenade' && (
                  <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">GRENADE PARAMS</span>
                    {GRENADE_FIELDS.map(({ key, label }) =>
                      currentWeapon[key] !== undefined ? (
                        <div key={key} className="flex justify-between items-center">
                          <span className="text-[10px] font-mono text-[#7aaa7a]">{label}</span>
                          <span className="text-xs font-mono text-[#d1fad7]">{String(currentWeapon[key])}</span>
                        </div>
                      ) : null
                    )}
                  </section>
                )}

                {/* Rocket-specific fields */}
                {currentWeapon.projectileType === 'rocket' && (
                  <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">ROCKET PARAMS</span>
                    {ROCKET_FIELDS.map(({ key, label }) =>
                      currentWeapon[key] !== undefined ? (
                        <div key={key} className="flex justify-between items-center">
                          <span className="text-[10px] font-mono text-[#7aaa7a]">{label}</span>
                          <span className="text-xs font-mono text-[#d1fad7]">{String(currentWeapon[key])}</span>
                        </div>
                      ) : null
                    )}
                  </section>
                )}

                {/* Plasma-specific fields */}
                {currentWeapon.projectileType === 'plasma' && (
                  <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">PLASMA PARAMS</span>
                    {PLASMA_FIELDS.map(({ key, label }) =>
                      currentWeapon[key] !== undefined ? (
                        <div key={key} className="flex justify-between items-center">
                          <span className="text-[10px] font-mono text-[#7aaa7a]">{label}</span>
                          <span className="text-xs font-mono text-[#d1fad7]">{String(currentWeapon[key])}</span>
                        </div>
                      ) : null
                    )}
                  </section>
                )}

                {/* Sprite banks */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <div className="flex items-center justify-between">
                    <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">SPRITE BANKS</span>
                    <Link href="/sprites" className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors">
                      BROWSE →
                    </Link>
                  </div>
                  <div className="grid grid-cols-2 gap-2">
                    {DIRECTIONS.map((dir, i) => {
                      const val = (currentWeapon.spriteBanks ?? [])[i] ?? 0xFF;
                      return (
                        <div key={dir} className="flex items-center gap-2">
                          <span className="text-[9px] font-mono text-[#4a7a4a] w-8">{dir}</span>
                          <input
                            type="number" min={0} max={255}
                            value={val === 0xFF ? '' : val}
                            placeholder="—"
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
                      type="number" min={-1} max={255}
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
                    <Link href="/sound-studio" className="text-[9px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors">
                      BROWSE →
                    </Link>
                  </div>
                  {soundFieldsForWeapon(currentWeapon).length === 0 ? (
                    <p className="text-[9px] font-mono text-[#2a4a2a]">No sounds for this projectile type.</p>
                  ) : soundFieldsForWeapon(currentWeapon).map(({ key, label }) => {
                    const val = String(currentWeapon[key] ?? '');
                    return (
                      <div key={String(key)} className="flex items-center gap-2">
                        <span className="text-[9px] font-mono text-[#4a7a4a] w-16 shrink-0">{label}</span>
                        <select
                          value={val}
                          onChange={e => patchWeapon(currentWeapon.id, { [key]: e.target.value })}
                          className="flex-1 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded focus:border-[#00a328] outline-none"
                        >
                          <option value="">— none —</option>
                          {soundList.map(name => (
                            <option key={name} value={name}>{name}</option>
                          ))}
                          {val && !soundList.includes(val) && (
                            <option value={val}>{val}</option>
                          )}
                        </select>
                        {val && (
                          <button
                            title={`Play ${val}`}
                            onClick={() => playSound(val)}
                            className="shrink-0 text-[10px] font-mono text-[#4a7a4a] hover:text-[#00a328] transition-colors px-1"
                          >
                            {playingSound === val ? '■' : '▶'}
                          </button>
                        )}
                      </div>
                    );
                  })}
                </section>

                {/* Agency loadout */}
                <section className="border border-[#1a2e1a] rounded p-4 flex flex-col gap-3">
                  <span className="text-[10px] font-mono text-[#4a7a4a] tracking-widest">AGENCY LOADOUT</span>
                  <p className="text-[9px] text-[#4a7a4a] font-mono">Which agencies can equip this weapon.</p>
                  {folder.agencies.map(agency => {
                    const has = (agency.weapons ?? []).includes(currentWeapon.id);
                    return (
                      <label
                        key={agency.id}
                        onClick={() => toggleAgencyWeapon(agency.id, currentWeapon.id)}
                        className="flex items-center gap-3 cursor-pointer group"
                      >
                        <div className={`w-3 h-3 border flex items-center justify-center shrink-0 transition-colors ${
                          has ? 'border-[#00a328] bg-[#00a328]' : 'border-[#1a2e1a] group-hover:border-[#4a7a4a]'
                        }`}>
                          {has && <span className="text-[8px] text-black leading-none">✓</span>}
                        </div>
                        <span className="text-xs font-mono text-[#7aaa7a] group-hover:text-[#d1fad7] transition-colors">
                          {agency.name ?? `Agency ${agency.id}`}
                        </span>
                      </label>
                    );
                  })}
                </section>

                {/* Ballistics preview */}
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

// ── Ballistics preview ───────────────────────────────────────────────────────

const TICKS_PER_SEC = 24;

function buildPath(weapon: WeaponDef, W: number, H: number): [number, number][] {
  const type = weapon.projectileType ?? 'physics';
  const velocityRaw = weapon.velocity as number | undefined;
  const velocity = velocityRaw ?? (type === 'physics' ? 20 : 0);
  const gravity = (weapon as Record<string, unknown>).plasmaGravity as number | undefined
    ?? (type === 'grenade' || type === 'arcing' ? 1.5 : 0);
  const moveAmount = weapon.moveAmount ?? 1;
  const scale = 1.5;
  const isThrow = type === 'grenade' || type === 'arcing';

  let x = 20, y = isThrow ? H - 20 : H * 0.6;
  let xv = velocity * scale;
  let yv = 0;
  const path: [number, number][] = [[x, y]];

  if (isThrow) {
    const throwSpeed = (weapon as Record<string, unknown>).throwSpeedStanding as number ?? 20;
    xv = throwSpeed * scale * 0.5;
    yv = -10 * scale;
  }

  if (velocity === 0 && !isThrow) return path;

  for (let step = 0; step < 800; step++) {
    const steps = Math.max(1, moveAmount);
    for (let m = 0; m < steps; m++) {
      x += xv / steps;
      y += yv / steps;
    }
    if (gravity) yv += gravity * 0.4;
    if (y >= H - 20 && step > 0) break;
    if (x > W) break;
    path.push([x, y]);
  }
  return path;
}

function drawFrame(
  ctx: CanvasRenderingContext2D,
  weapon: WeaponDef,
  path: [number, number][],
  tickIdx: number,
  W: number,
  H: number,
) {
  const type = weapon.projectileType ?? 'physics';
  const velocityRaw = weapon.velocity as number | undefined;
  const velocity = velocityRaw ?? (type === 'physics' ? 20 : 0);
  const gravity = (weapon as Record<string, unknown>).plasmaGravity as number | undefined
    ?? (type === 'grenade' || type === 'arcing' ? 1.5 : 0);
  const radius = weapon.radius ?? 0;
  const scale = 1.5;

  ctx.fillStyle = '#080f08';
  ctx.fillRect(0, 0, W, H);
  ctx.strokeStyle = '#1a2e1a';
  ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(0, H - 20); ctx.lineTo(W, H - 20); ctx.stroke();

  if (path.length <= 1 && velocity === 0 && type !== 'grenade' && type !== 'arcing') {
    ctx.fillStyle = '#2a4a2a';
    ctx.font = '10px monospace';
    ctx.fillText('no velocity — contact / fixed-direction', 10, H / 2);
    return;
  }

  const end = Math.min(tickIdx + 1, path.length);
  const visible = path.slice(0, end);

  ctx.strokeStyle = '#00a32888';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  visible.forEach(([px, py], i) => i === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py));
  ctx.stroke();

  const [cx, cy] = visible[visible.length - 1];
  ctx.fillStyle = '#00a328';
  ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI * 2); ctx.fill();

  // Show blast radius only at final rest
  if (tickIdx >= path.length - 1 && radius > 0) {
    const [ix, iy] = path[path.length - 1];
    ctx.strokeStyle = '#f59e0b44';
    ctx.fillStyle = '#f59e0b22';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.arc(ix, iy, radius * scale * 0.5, 0, Math.PI * 2);
    ctx.fill(); ctx.stroke();
  }

  const label = [
    velocityRaw !== undefined ? `v=${velocity}` : `v≈${velocity}(est)`,
    `steps=${weapon.moveAmount ?? 1}`,
    gravity ? `g=${gravity}` : null,
    radius ? `r=${radius}px` : null,
    `tick ${Math.min(tickIdx, path.length - 1)}/${path.length - 1}`,
  ].filter(Boolean).join('  ');
  ctx.fillStyle = '#4a7a4a';
  ctx.font = '9px monospace';
  ctx.fillText(label, 6, 12);
}

function BallisticsPreview({ weapon }: { weapon: WeaponDef }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [playing, setPlaying] = useState(false);
  const tickRef = useRef(0);
  const rafRef = useRef<number | null>(null);
  const lastTimeRef = useRef<number | null>(null);
  const pathRef = useRef<[number, number][]>([]);

  const W = 560, H = 160;

  const stopAnim = useCallback(() => {
    if (rafRef.current !== null) { cancelAnimationFrame(rafRef.current); rafRef.current = null; }
    lastTimeRef.current = null;
    setPlaying(false);
  }, []);

  const drawStatic = useCallback((atTick?: number) => {
    const cvs = canvasRef.current;
    if (!cvs) return;
    const ctx = cvs.getContext('2d');
    if (!ctx) return;
    const path = pathRef.current;
    drawFrame(ctx, weapon, path, atTick ?? path.length - 1, W, H);
  }, [weapon]);

  // Rebuild path whenever weapon changes; redraw static frame
  useEffect(() => {
    pathRef.current = buildPath(weapon, W, H);
    tickRef.current = pathRef.current.length - 1;
    stopAnim();
    drawStatic();
  }, [weapon, stopAnim, drawStatic]);

  const startAnim = useCallback(() => {
    const path = pathRef.current;
    if (path.length === 0) return;
    tickRef.current = 0;
    lastTimeRef.current = null;
    setPlaying(true);

    const loop = (now: number) => {
      if (lastTimeRef.current === null) lastTimeRef.current = now;
      const elapsed = now - lastTimeRef.current;
      const ticksElapsed = Math.floor(elapsed / (1000 / TICKS_PER_SEC));
      if (ticksElapsed > 0) {
        tickRef.current = Math.min(tickRef.current + ticksElapsed, path.length - 1);
        lastTimeRef.current = now - (elapsed % (1000 / TICKS_PER_SEC));
        const cvs = canvasRef.current;
        const ctx = cvs?.getContext('2d');
        if (ctx) drawFrame(ctx, weapon, path, tickRef.current, W, H);
        if (tickRef.current >= path.length - 1) { stopAnim(); return; }
      }
      rafRef.current = requestAnimationFrame(loop);
    };
    rafRef.current = requestAnimationFrame(loop);
  }, [weapon, stopAnim]);

  return (
    <div className="flex flex-col gap-2">
      <canvas
        ref={canvasRef}
        width={W}
        height={H}
        className="w-full border border-[#1a2e1a] rounded"
        style={{ imageRendering: 'pixelated' }}
      />
      <div className="flex gap-2">
        {!playing ? (
          <button
            onClick={startAnim}
            className="px-3 py-1 text-[10px] font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:border-[#00a328] hover:text-[#00a328] transition-colors"
          >
            ▶ PLAY
          </button>
        ) : (
          <button
            onClick={stopAnim}
            className="px-3 py-1 text-[10px] font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:border-[#00a328] hover:text-[#00a328] transition-colors"
          >
            ■ STOP
          </button>
        )}
        <button
          onClick={() => { stopAnim(); drawStatic(); }}
          className="px-3 py-1 text-[10px] font-mono border border-[#1a2e1a] text-[#4a7a4a] hover:border-[#00a328] hover:text-[#00a328] transition-colors"
        >
          ↺ RESET
        </button>
      </div>
    </div>
  );
}
