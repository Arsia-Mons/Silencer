'use client';
import { useEffect, useRef, useState, useCallback } from 'react';
import Link from 'next/link';
import { useAuth } from '../../../lib/auth';
import { useWsConnected } from '../../../lib/socket';
import Sidebar from '../../../components/Sidebar';
import { getSpriteBanks, getSpriteFrames, type BankInfo, type FrameMeta } from '../../../lib/api';

/** Small component that lazy-loads a sprite PNG once it enters the viewport. */
function SpriteThumb({
  bank, frame, selected, onClick,
}: {
  bank: number; frame: number; selected: boolean; onClick: () => void;
}) {
  const ref = useRef<HTMLDivElement>(null);
  const [src, setSrc] = useState<string | null>(null);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const obs = new IntersectionObserver(([entry]) => {
      if (entry.isIntersecting) {
        const token = localStorage.getItem('zs_token') ?? '';
        // Use the Next.js proxy so we can pass auth
        setSrc(`/api/sprites/${bank}/${frame}?_t=${token.slice(-8)}`);
        obs.disconnect();
      }
    }, { rootMargin: '200px' });
    obs.observe(el);
    return () => obs.disconnect();
  }, [bank, frame]);

  return (
    <div
      ref={ref}
      onClick={onClick}
      title={`Bank ${bank} Frame ${frame}`}
      className={`cursor-pointer border flex items-center justify-center bg-black/40 min-h-[50px] min-w-[50px] transition-colors ${
        selected
          ? 'border-game-primary ring-1 ring-game-primary'
          : 'border-game-border hover:border-game-textDim'
      }`}
      style={{ width: 70, height: 70 }}
    >
      {src ? (
        <img
          src={src}
          alt={`bank${bank}:${frame}`}
          className="max-w-full max-h-full object-contain"
          style={{ imageRendering: 'pixelated' }}
        />
      ) : (
        <span className="text-game-border text-xs">{frame}</span>
      )}
    </div>
  );
}

export default function SpriteBrowserPage() {
  useAuth();
  const wsConnected = useWsConnected();

  const [banks, setBanks]           = useState<BankInfo[]>([]);
  const [selectedBank, setSelectedBank] = useState<number>(9);
  const [frames, setFrames]         = useState<FrameMeta[]>([]);
  const [selectedFrame, setSelectedFrame] = useState<number | null>(null);
  const [loading, setLoading]       = useState(false);
  const [copied, setCopied]         = useState(false);

  // Load bank list once
  useEffect(() => {
    getSpriteBanks().then(setBanks).catch(console.error);
  }, []);

  // Load frames when bank changes
  const loadFrames = useCallback(async (bank: number) => {
    setLoading(true);
    setSelectedFrame(null);
    try {
      setFrames(await getSpriteFrames(bank));
    } catch (e) {
      console.error(e);
      setFrames([]);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { loadFrames(selectedBank); }, [selectedBank, loadFrames]);

  function handleCopy() {
    if (selectedFrame === null) return;
    navigator.clipboard.writeText(`${selectedBank}:${selectedFrame}`);
    setCopied(true);
    setTimeout(() => setCopied(false), 1500);
  }

  const selectedMeta = selectedFrame !== null ? frames[selectedFrame] : null;

  return (
    <div className="flex min-h-screen bg-game-bg text-game-text">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-8 flex flex-col">
        {/* Header */}
        <div className="flex items-center gap-4 mb-6">
          <Link href="/actors" className="text-game-textDim hover:text-game-text text-sm">← ACTORS</Link>
          <h1 className="text-2xl font-bold tracking-widest text-game-primary">SPRITE BROWSER</h1>
        </div>

        <div className="flex gap-6 flex-1 min-h-0">
          {/* Bank selector */}
          <div className="w-44 flex flex-col">
            <div className="text-xs text-game-textDim tracking-widest mb-2">BANK ({banks.length})</div>
            <div className="flex-1 overflow-y-auto border border-game-border">
              {banks.map(b => (
                <button
                  key={b.bank}
                  onClick={() => setSelectedBank(b.bank)}
                  className={`w-full text-left px-3 py-1.5 text-sm font-mono flex justify-between items-center ${
                    b.bank === selectedBank
                      ? 'bg-game-primary text-black'
                      : 'hover:bg-game-bgCard text-game-text'
                  }`}
                >
                  <span>{String(b.bank).padStart(3, '0')}</span>
                  <span className="text-xs opacity-60">{b.frames}f</span>
                </button>
              ))}
            </div>
          </div>

          {/* Frame grid */}
          <div className="flex-1 flex flex-col min-w-0">
            <div className="flex items-center justify-between mb-2">
              <div className="text-xs text-game-textDim tracking-widest">
                BANK {selectedBank} — {frames.length} FRAMES
              </div>
              {selectedFrame !== null && (
                <button
                  onClick={handleCopy}
                  className="text-xs px-3 py-1 border border-game-border hover:border-game-primary text-game-textDim hover:text-game-text"
                >
                  {copied ? '✓ COPIED' : `COPY ${selectedBank}:${selectedFrame}`}
                </button>
              )}
            </div>

            {loading ? (
              <div className="text-game-textDim text-sm py-8">Loading frames…</div>
            ) : (
              <div className="flex-1 overflow-y-auto">
                <div className="flex flex-wrap gap-1">
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
              </div>
            )}
          </div>

          {/* Info panel */}
          <div className="w-52 flex flex-col gap-4">
            <div className="text-xs text-game-textDim tracking-widest mb-2">FRAME INFO</div>
            {selectedMeta ? (
              <div className="bg-game-bgCard border border-game-border p-4 space-y-2 text-sm font-mono">
                <div className="flex justify-between">
                  <span className="text-game-textDim">Bank</span>
                  <span>{selectedBank}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-game-textDim">Frame</span>
                  <span>{selectedMeta.frame}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-game-textDim">Size</span>
                  <span>{selectedMeta.width}×{selectedMeta.height}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-game-textDim">OffsetX</span>
                  <span>{selectedMeta.offsetX}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-game-textDim">OffsetY</span>
                  <span>{selectedMeta.offsetY}</span>
                </div>
                <div className="mt-4 border-t border-game-border pt-3">
                  <div className="text-game-textDim text-xs mb-2">PREVIEW</div>
                  <div className="bg-black flex items-center justify-center" style={{ minHeight: 80 }}>
                    <img
                      src={`/api/sprites/${selectedBank}/${selectedMeta.frame}`}
                      alt=""
                      className="max-w-full max-h-32 object-contain"
                      style={{ imageRendering: 'pixelated' }}
                    />
                  </div>
                </div>
              </div>
            ) : (
              <div className="text-game-textDim text-xs text-center py-8">
                Click a frame to inspect
              </div>
            )}
          </div>
        </div>
      </main>
    </div>
  );
}
