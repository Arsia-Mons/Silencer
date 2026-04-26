'use client';
import { useEffect, useRef, useState, useCallback, useLayoutEffect } from 'react';
import Link from 'next/link';
import { useAuth } from '../../../lib/auth';
import { useWsConnected } from '../../../lib/socket';
import Sidebar from '../../../components/Sidebar';
import { getSpriteBanks, getSpriteFrames, type BankInfo, type FrameMeta } from '../../../lib/api';

const SCALES = [1, 2, 4, 8] as const;
type Scale = typeof SCALES[number];

/** Checkerboard background to visualise transparency. */
const CHECKER = `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16'%3E%3Crect width='8' height='8' fill='%23222'/%3E%3Crect x='8' y='8' width='8' height='8' fill='%23222'/%3E%3Crect x='8' width='8' height='8' fill='%23333'/%3E%3Crect y='8' width='8' height='8' fill='%23333'/%3E%3C/svg%3E")`;

function SpriteThumb({
  bank, frame, selected, onClick,
}: {
  bank: number; frame: number; selected: boolean; onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      title={`Frame ${frame}`}
      className={`flex flex-col items-center gap-0.5 p-1 border transition-colors focus:outline-none ${
        selected
          ? 'border-game-primary bg-game-primary/10'
          : 'border-game-border hover:border-game-textDim bg-black/30'
      }`}
      style={{ width: 88, minHeight: 80 }}
    >
      <div
        className="flex items-center justify-center flex-1 w-full"
        style={{ background: CHECKER, minHeight: 64 }}
      >
        {/* eslint-disable-next-line @next/next/no-img-element */}
        <img
          src={`/api/sprites/${bank}/${frame}`}
          alt={`b${bank}f${frame}`}
          style={{ imageRendering: 'pixelated', maxWidth: 80, maxHeight: 72 }}
        />
      </div>
      <span className="text-[10px] font-mono text-game-textDim leading-none">{frame}</span>
    </button>
  );
}

export default function SpriteBrowserPage() {
  useAuth();
  const wsConnected = useWsConnected();

  const [banks, setBanks]               = useState<BankInfo[]>([]);
  const [bankSearch, setBankSearch]     = useState('');
  const [selectedBank, setSelectedBank] = useState<number>(9);
  const [frames, setFrames]             = useState<FrameMeta[]>([]);
  const [selectedFrame, setSelectedFrame] = useState<number>(0);
  const [loading, setLoading]           = useState(false);
  const [scale, setScale]               = useState<Scale>(4);
  const [copied, setCopied]             = useState(false);
  const selectedBankRef                 = useRef<HTMLButtonElement>(null);

  useEffect(() => { getSpriteBanks().then(setBanks).catch(console.error); }, []);

  const loadFrames = useCallback(async (bank: number) => {
    setLoading(true);
    setSelectedFrame(0);
    try { setFrames(await getSpriteFrames(bank)); }
    catch { setFrames([]); }
    finally { setLoading(false); }
  }, []);

  useEffect(() => { loadFrames(selectedBank); }, [selectedBank, loadFrames]);

  // Keep selected bank visible in list when filter changes
  useLayoutEffect(() => {
    selectedBankRef.current?.scrollIntoView({ block: 'nearest' });
  }, [selectedBank, bankSearch]);

  // Keyboard navigation between frames
  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      if (e.target instanceof HTMLInputElement) return;
      if (e.key === 'ArrowRight') setSelectedFrame(f => Math.min(f + 1, frames.length - 1));
      if (e.key === 'ArrowLeft')  setSelectedFrame(f => Math.max(f - 1, 0));
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [frames.length]);

  function handleCopy() {
    navigator.clipboard.writeText(`${selectedBank}:${selectedFrame}`);
    setCopied(true);
    setTimeout(() => setCopied(false), 1500);
  }

  const filteredBanks = banks.filter(b =>
    bankSearch === '' || String(b.bank).includes(bankSearch.trim())
  );

  const meta = frames[selectedFrame] ?? null;

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 flex flex-col overflow-hidden" style={{ height: '100vh' }}>

        {/* Top bar */}
        <div className="flex items-center gap-4 px-6 py-4 border-b border-game-border shrink-0">
          <Link href="/actors" className="text-game-textDim hover:text-game-text text-sm">← ACTORS</Link>
          <h1 className="text-xl font-bold tracking-widest text-game-primary">SPRITE BROWSER</h1>
          <span className="text-game-textDim text-xs font-mono ml-auto">
            {banks.length} banks loaded
          </span>
        </div>

        <div className="flex flex-1 min-h-0">

          {/* ── Bank list ───────────────────────────────────────────── */}
          <div className="w-40 flex flex-col border-r border-game-border shrink-0">
            <div className="p-2 border-b border-game-border">
              <input
                type="text"
                placeholder="Filter bank…"
                value={bankSearch}
                onChange={e => setBankSearch(e.target.value)}
                className="w-full bg-game-bgCard border border-game-border text-game-text text-xs font-mono px-2 py-1 focus:outline-none focus:border-game-primary"
              />
            </div>
            <div className="flex-1 overflow-y-auto">
              {filteredBanks.map(b => (
                <button
                  key={b.bank}
                  ref={b.bank === selectedBank ? selectedBankRef : undefined}
                  onClick={() => setSelectedBank(b.bank)}
                  className={`w-full text-left px-3 py-1.5 text-xs font-mono flex justify-between items-center transition-colors ${
                    b.bank === selectedBank
                      ? 'bg-game-primary text-black font-bold'
                      : 'hover:bg-game-bgCard text-game-text'
                  }`}
                >
                  <span>{String(b.bank).padStart(3, '0')}</span>
                  <span className="opacity-60">{b.frames}f</span>
                </button>
              ))}
              {filteredBanks.length === 0 && (
                <div className="text-game-textDim text-xs text-center py-6">No match</div>
              )}
            </div>
          </div>

          {/* ── Frame grid + preview ─────────────────────────────────── */}
          <div className="flex-1 flex flex-col min-w-0">

            {/* Frame strip */}
            <div className="border-b border-game-border shrink-0">
              <div className="flex items-center gap-3 px-4 py-2 border-b border-game-border/50">
                <span className="text-xs font-mono text-game-textDim">
                  BANK <span className="text-game-text font-bold">{selectedBank}</span>
                  {' · '}{frames.length} frames
                  {frames.length > 0 && (
                    <span className="ml-2 text-game-textDim">
                      ← → to navigate
                    </span>
                  )}
                </span>
              </div>
              {loading ? (
                <div className="text-game-textDim text-xs px-4 py-6">Loading…</div>
              ) : (
                <div className="flex gap-1 overflow-x-auto p-2" style={{ maxHeight: 120 }}>
                  {frames.map(f => (
                    <SpriteThumb
                      key={f.frame}
                      bank={selectedBank}
                      frame={f.frame}
                      selected={selectedFrame === f.frame}
                      onClick={() => setSelectedFrame(f.frame)}
                    />
                  ))}
                </div>
              )}
            </div>

            {/* Large preview */}
            <div className="flex-1 flex min-h-0">
              <div className="flex-1 flex items-center justify-center min-h-0" style={{ background: CHECKER }}>
                {meta && (
                  // eslint-disable-next-line @next/next/no-img-element
                  <img
                    key={`${selectedBank}-${selectedFrame}-${scale}`}
                    src={`/api/sprites/${selectedBank}/${selectedFrame}`}
                    alt=""
                    style={{
                      imageRendering: 'pixelated',
                      width:  meta.width  * scale,
                      height: meta.height * scale,
                      maxWidth: '100%',
                      maxHeight: '100%',
                    }}
                  />
                )}
              </div>

              {/* Info sidebar */}
              <div className="w-52 border-l border-game-border flex flex-col shrink-0">
                <div className="p-3 border-b border-game-border">
                  <div className="text-[10px] text-game-textDim tracking-widest mb-2">ZOOM</div>
                  <div className="flex gap-1">
                    {SCALES.map(s => (
                      <button
                        key={s}
                        onClick={() => setScale(s)}
                        className={`flex-1 py-1 text-xs font-mono border transition-colors ${
                          s === scale
                            ? 'border-game-primary bg-game-primary text-black'
                            : 'border-game-border hover:border-game-textDim text-game-textDim'
                        }`}
                      >
                        {s}×
                      </button>
                    ))}
                  </div>
                </div>

                {meta ? (
                  <div className="p-3 space-y-2 text-xs font-mono flex-1">
                    <Row label="Bank"    value={selectedBank} />
                    <Row label="Frame"   value={`${selectedFrame} / ${frames.length - 1}`} />
                    <Row label="Size"    value={`${meta.width} × ${meta.height}`} />
                    <Row label="OffsetX" value={meta.offsetX} />
                    <Row label="OffsetY" value={meta.offsetY} />
                  </div>
                ) : (
                  <div className="flex-1" />
                )}

                <div className="p-3 border-t border-game-border space-y-2">
                  <button
                    onClick={handleCopy}
                    disabled={!meta}
                    className="w-full py-2 text-xs font-mono border border-game-border hover:border-game-primary text-game-textDim hover:text-game-text disabled:opacity-30 transition-colors"
                  >
                    {copied ? '✓ COPIED' : `COPY ${selectedBank}:${selectedFrame}`}
                  </button>
                </div>
              </div>
            </div>
          </div>
        </div>
      </main>
    </div>
  );
}

function Row({ label, value }: { label: string; value: string | number }) {
  return (
    <div className="flex justify-between items-baseline">
      <span className="text-game-textDim">{label}</span>
      <span className="text-game-text">{value}</span>
    </div>
  );
}
