'use client';
import { useRef, useState, useCallback, useEffect, Suspense } from 'react';
import { useRouter, useSearchParams } from 'next/navigation';
import { useAuth } from '../../lib/auth';
import { useWsConnected } from '../../lib/socket';
import Sidebar from '../../components/Sidebar';
import { zipSync } from 'fflate';
import {
  parseDat,
  decodeBank,
  decodeTileBank,
  loadPalette,
  frameToImageData,
  encodeBank,
  encodeTileBank,
  encodeDat,
  quantizeToPalette,
  type DecodedBank,
  type DecodedFrame,
} from '../../lib/spriteDecoder';

// ── Constants ────────────────────────────────────────────────────────────────

const TABS = ['sprites', 'tiles'] as const;
type TabKey = (typeof TABS)[number];

const DAT_NAME: Record<TabKey, string> = {
  sprites: 'BIN_SPR.DAT',
  tiles:   'BIN_TIL.DAT',
};
const DIR_NAME: Record<TabKey, string> = {
  sprites: 'bin_spr',
  tiles:   'bin_til',
};
const BIN_PREFIX: Record<TabKey, string> = {
  sprites: 'SPR_',
  tiles:   'TIL_',
};

const ACCENT    = '#00a328';
const THUMB_W   = 80;
const THUMB_H   = 80;

// ── Folder state ─────────────────────────────────────────────────────────────

interface TabAssets {
  datBuf:      ArrayBuffer;
  frameCounts: number[];             // [256]
  bankFiles:   Map<number, ArrayBuffer>;
}

interface FolderState {
  sprites:    TabAssets | null;
  tiles:      TabAssets | null;
  paletteBuf: ArrayBuffer | null;
}

// ── Per-bank edit state ──────────────────────────────────────────────────────

// ── Helper: bank filename ─────────────────────────────────────────────────────

function bankFilename(tab: TabKey, bankIndex: number): string {
  return `${BIN_PREFIX[tab]}${String(bankIndex).padStart(3, '0')}.BIN`;
}

// ── Thumbnail canvas ──────────────────────────────────────────────────────────

function Thumbnail({
  frame,
  palette,
  selected,
  onClick,
}: {
  frame: DecodedFrame;
  palette: Uint8Array;
  selected: boolean;
  onClick: () => void;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const imgData = frameToImageData(frame, palette);
    // Draw into offscreen canvas then scale
    const tmp = document.createElement('canvas');
    tmp.width = frame.header.width;
    tmp.height = frame.header.height;
    const tc = tmp.getContext('2d');
    if (!tc) return;
    tc.putImageData(imgData, 0, 0);
    ctx.clearRect(0, 0, THUMB_W, THUMB_H);
    ctx.imageSmoothingEnabled = false;
    // Scale to fit
    const scale = Math.min(THUMB_W / frame.header.width, THUMB_H / frame.header.height);
    const dw = Math.round(frame.header.width * scale);
    const dh = Math.round(frame.header.height * scale);
    const dx = Math.round((THUMB_W - dw) / 2);
    const dy = Math.round((THUMB_H - dh) / 2);
    ctx.drawImage(tmp, dx, dy, dw, dh);
  }, [frame, palette]);

  return (
    <button
      onClick={onClick}
      className={`relative border rounded p-1 transition-colors ${
        selected
          ? 'border-[#00a328] bg-[#0a1a0a]'
          : 'border-[#1a2e1a] bg-[#080f08] hover:border-[#2a4a2a]'
      }`}
      style={{ width: THUMB_W + 10, height: THUMB_H + 10 }}
    >
      <canvas ref={canvasRef} width={THUMB_W} height={THUMB_H} />
      {frame.dirty && (
        <span className="absolute top-0.5 right-0.5 w-1.5 h-1.5 rounded-full bg-[#f59e0b]" />
      )}
    </button>
  );
}

// ── Full-size frame canvas ────────────────────────────────────────────────────

function FrameCanvas({
  frame,
  palette,
}: {
  frame: DecodedFrame;
  palette: Uint8Array;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const imgData = frameToImageData(frame, palette);
    canvas.width = frame.header.width;
    canvas.height = frame.header.height;
    ctx.putImageData(imgData, 0, 0);
  }, [frame, palette]);

  return (
    <div
      className="overflow-auto border border-[#1a2e1a] rounded"
      style={{ maxWidth: 480, maxHeight: 400, background: '#050a05' }}
    >
      <canvas
        ref={canvasRef}
        width={frame.header.width}
        height={frame.header.height}
        style={{ imageRendering: 'pixelated', display: 'block' }}
      />
    </div>
  );
}

// ── Inner page (inside Suspense) ──────────────────────────────────────────────

function SpritesPageInner() {
  useAuth();
  const wsConnected = useWsConnected();
  const router = useRouter();
  const searchParams = useSearchParams();

  // Tab state from URL
  const tabParam = searchParams.get('tab') as TabKey | null;
  const tab: TabKey = TABS.includes(tabParam as TabKey) ? (tabParam as TabKey) : 'sprites';

  function switchTab(t: TabKey) {
    const p = new URLSearchParams(searchParams.toString());
    p.set('tab', t);
    router.replace(`?${p.toString()}`, { scroll: false });
  }

  // ── Folder open ──────────────────────────────────────────────────────────
  const folderInputRef = useRef<HTMLInputElement>(null);
  const [folder, setFolder] = useState<FolderState | null>(null);
  const [folderName, setFolderName] = useState<string | null>(null);

  async function loadTabAssets(
    all: File[],
    t: TabKey,
  ): Promise<TabAssets | null> {
    const datFile = all.find(f => {
      const parts = f.webkitRelativePath.split('/');
      return parts[parts.length - 1].toUpperCase() === DAT_NAME[t];
    });
    if (!datFile) return null;
    const datBuf = await datFile.arrayBuffer();
    const frameCounts = parseDat(datBuf);
    const binDir = DIR_NAME[t].toLowerCase();
    const binFiles = all.filter(f => {
      const lower = f.webkitRelativePath.toLowerCase();
      return lower.includes('/' + binDir + '/') && lower.endsWith('.bin');
    });
    const bankFiles = new Map<number, ArrayBuffer>();
    await Promise.all(
      binFiles.map(async f => {
        const fname = f.name.toUpperCase();
        const prefix = BIN_PREFIX[t].toUpperCase();
        if (!fname.startsWith(prefix) || !fname.endsWith('.BIN')) return;
        const numStr = fname.slice(prefix.length, fname.length - 4);
        const idx = parseInt(numStr, 10);
        if (isNaN(idx) || idx < 0 || idx > 255) return;
        bankFiles.set(idx, await f.arrayBuffer());
      }),
    );
    return { datBuf, frameCounts, bankFiles };
  }

  async function openFolder(files: FileList) {
    const all = Array.from(files);
    const paletteFile = all.find(f => {
      const parts = f.webkitRelativePath.split('/');
      return parts[parts.length - 1].toUpperCase() === 'PALETTE.BIN';
    });

    const [sprites, tiles, palBuf] = await Promise.all([
      loadTabAssets(all, 'sprites'),
      loadTabAssets(all, 'tiles'),
      paletteFile ? paletteFile.arrayBuffer() : Promise.resolve(null),
    ]);

    if (!sprites && !tiles) {
      alert('No BIN_SPR.DAT or BIN_TIL.DAT found in the selected folder.');
      return;
    }

    const folderRoot = all[0]?.webkitRelativePath?.split('/')[0] ?? 'assets';
    setFolderName(folderRoot);
    setFolder({ sprites, tiles, paletteBuf: palBuf });
    setDecodedBanks(new Map());
    setSelectedBank(null);
    setSelectedFrame(null);
    setDeletedBanks([]);
    setDirtyDat(false);
  }

  function handleFolderInput(e: React.ChangeEvent<HTMLInputElement>) {
    if (!e.target.files || e.target.files.length === 0) return;
    openFolder(e.target.files);
    e.target.value = '';
  }

  function handleCloseFolder() {
    setFolder(null);
    setFolderName(null);
    setDecodedBanks(new Map());
    setSelectedBank(null);
    setSelectedFrame(null);
    setDeletedBanks([]);
    setDirtyDat(false);
  }

  // ── Decoded bank cache ────────────────────────────────────────────────────
  const [decodedBanks, setDecodedBanks] = useState<Map<number, DecodedBank>>(new Map());
  const [selectedBank, setSelectedBank] = useState<number | null>(null);
  const [selectedFrame, setSelectedFrame] = useState<number | null>(null);
  const [subPalette, setSubPalette] = useState(0);
  const [deletedBanks, setDeletedBanks] = useState<number[]>([]);
  const [dirtyDat, setDirtyDat] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  const [draggingFrameIdx, setDraggingFrameIdx] = useState<number | null>(null);

  // Current tab's assets
  const tabAssets: TabAssets | null = folder ? (tab === 'sprites' ? folder.sprites : folder.tiles) : null;

  // Clear decoded bank cache when switching tabs
  useEffect(() => {
    setDecodedBanks(new Map());
    setSelectedBank(null);
    setSelectedFrame(null);
  }, [tab]);

  // ── Palette ───────────────────────────────────────────────────────────────
  const [palette, setPalette] = useState<Uint8Array>(() => new Uint8Array(256 * 4));

  useEffect(() => {
    if (!folder?.paletteBuf) return;
    setPalette(loadPalette(folder.paletteBuf, subPalette));
  }, [folder?.paletteBuf, subPalette]);

  // ── Occupied banks list ───────────────────────────────────────────────────
  const occupiedBanks: Array<{ idx: number; count: number }> = tabAssets
    ? tabAssets.frameCounts
        .map((count, idx) => ({ idx, count }))
        .filter(b => b.count > 0 && !deletedBanks.includes(b.idx))
    : [];

  // ── Lazy decode on bank select ────────────────────────────────────────────
  const selectBank = useCallback(
    (idx: number) => {
      setSelectedBank(idx);
      setSelectedFrame(null);
      if (!tabAssets) return;
      if (decodedBanks.has(idx)) return;
      const buf = tabAssets.bankFiles.get(idx);
      if (!buf) return;
      const numFrames = tabAssets.frameCounts[idx] ?? 0;
      try {
        const bank = tab === 'tiles'
          ? decodeTileBank(idx, buf, numFrames)
          : decodeBank(idx, buf, numFrames);
        setDecodedBanks(prev => new Map(prev).set(idx, bank));
      } catch (e) {
        setError(`Failed to decode bank ${idx}: ${e instanceof Error ? e.message : String(e)}`);
      }
    },
    [tabAssets, decodedBanks, tab],
  );

  // ── Keyboard navigation through bank list ────────────────────────────────
  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      if (!occupiedBanks.length) return;
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) return;
      if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return;
      e.preventDefault();
      const pos = occupiedBanks.findIndex(b => b.idx === selectedBank);
      if (e.key === 'ArrowUp') {
        const next = pos <= 0 ? occupiedBanks[occupiedBanks.length - 1] : occupiedBanks[pos - 1];
        selectBank(next.idx);
      } else {
        const next = pos < 0 || pos >= occupiedBanks.length - 1 ? occupiedBanks[0] : occupiedBanks[pos + 1];
        selectBank(next.idx);
      }
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [occupiedBanks, selectedBank, selectBank]);

  const currentBank: DecodedBank | undefined =
    selectedBank !== null ? decodedBanks.get(selectedBank) : undefined;

  const currentFrame: DecodedFrame | undefined =
    currentBank && selectedFrame !== null
      ? currentBank.frames[selectedFrame]
      : undefined;

  // ── Anchor edit ───────────────────────────────────────────────────────────
  function patchFrameHeader(
    bankIdx: number,
    frameIdx: number,
    patch: Partial<Pick<DecodedFrame['header'], 'offsetX' | 'offsetY'>>,
  ) {
    setDecodedBanks(prev => {
      const m = new Map(prev);
      const bank = m.get(bankIdx);
      if (!bank) return prev;
      const frames = bank.frames.map((f, i) => {
        if (i !== frameIdx) return f;
        return {
          ...f,
          dirty: true,
          header: { ...f.header, ...patch },
        };
      });
      m.set(bankIdx, { ...bank, frames, dirty: true });
      return m;
    });
  }

  // ── Delete frame ──────────────────────────────────────────────────────────
  function deleteFrame(bankIdx: number, frameIdx: number) {
    if (!confirm(`Delete frame ${frameIdx} from bank ${bankIdx}?`)) return;
    setDecodedBanks(prev => {
      const m = new Map(prev);
      const bank = m.get(bankIdx);
      if (!bank) return prev;
      const frames = bank.frames.filter((_, i) => i !== frameIdx);
      m.set(bankIdx, { ...bank, frames, dirty: true });
      return m;
    });
    setSelectedFrame(null);
    // Update frameCounts
    if (folder && tabAssets) {
      const newCounts = [...tabAssets.frameCounts];
      newCounts[bankIdx] = Math.max(0, newCounts[bankIdx] - 1);
      setFolder(f => f ? { ...f, [tab]: { ...tabAssets, frameCounts: newCounts } } : f);
    }
    setDirtyDat(true);
  }

  // ── New bank ──────────────────────────────────────────────────────────────
  function handleNewBank() {
    if (!folder || !tabAssets) return;
    const input = prompt('Bank index (0–255):');
    if (input === null) return;
    const idx = parseInt(input, 10);
    if (isNaN(idx) || idx < 0 || idx > 255) { setError('Invalid bank index.'); return; }
    if (tabAssets.frameCounts[idx] > 0 && !deletedBanks.includes(idx)) {
      setError(`Bank ${idx} is already occupied.`);
      return;
    }
    const newBank: DecodedBank = { bankIndex: idx, frames: [], dirty: true };
    setDecodedBanks(prev => new Map(prev).set(idx, newBank));
    const newCounts = [...tabAssets.frameCounts];
    newCounts[idx] = 0;
    setFolder(f => f ? { ...f, [tab]: { ...tabAssets, frameCounts: newCounts } } : f);
    setDeletedBanks(d => d.filter(x => x !== idx));
    setSelectedBank(idx);
    setSelectedFrame(null);
    setDirtyDat(true);
  }

  // ── Delete bank ───────────────────────────────────────────────────────────
  function handleDeleteBank() {
    if (selectedBank === null) return;
    if (!confirm(`Delete bank ${selectedBank}? You must also manually git rm the .BIN file.`)) return;
    setDeletedBanks(d => [...d, selectedBank]);
    if (tabAssets) {
      const newCounts = [...tabAssets.frameCounts];
      newCounts[selectedBank] = 0;
      setFolder(f => f ? { ...f, [tab]: { ...tabAssets, frameCounts: newCounts } } : f);
    }
    setNotice(
      `Bank ${selectedBank} deleted. You must also run: git rm shared/assets/${DIR_NAME[tab]}/${bankFilename(tab, selectedBank)}`,
    );
    setSelectedBank(null);
    setSelectedFrame(null);
    setDirtyDat(true);
  }

  // ── Import PNG ────────────────────────────────────────────────────────────
  const pngInputRef = useRef<HTMLInputElement>(null);

  function handleImportPng(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file || selectedBank === null) return;
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => {
      URL.revokeObjectURL(url);
      if (img.width % 4 !== 0) {
        setError(`PNG width (${img.width}) must be a multiple of 4.`);
        return;
      }
      const cvs = document.createElement('canvas');
      cvs.width = img.width;
      cvs.height = img.height;
      const ctx = cvs.getContext('2d');
      if (!ctx) return;
      ctx.drawImage(img, 0, 0);
      const imageData = ctx.getImageData(0, 0, img.width, img.height);
      try {
        const indexed = quantizeToPalette(imageData, palette);
        const newFrame: DecodedFrame = {
          header: {
            width: img.width,
            height: img.height,
            offsetX: 0,
            offsetY: 0,
            compSize: 0,
            mode: 0,
            headerBytes: new Uint8Array(344),
          },
          indexedPixels: indexed,
          dirty: true,
        };
        setDecodedBanks(prev => {
          const m = new Map(prev);
          const bank = m.get(selectedBank);
          const frames = bank ? [...bank.frames, newFrame] : [newFrame];
          m.set(selectedBank, {
            bankIndex: selectedBank,
            frames,
            dirty: true,
          });
          return m;
        });
        const newCounts = [...(tabAssets?.frameCounts ?? new Array(256).fill(0))];
        newCounts[selectedBank] = (newCounts[selectedBank] ?? 0) + 1;
        setFolder(f => f && tabAssets ? { ...f, [tab]: { ...tabAssets, frameCounts: newCounts } } : f);
        setDirtyDat(true);
      } catch (err) {
        setError(err instanceof Error ? err.message : String(err));
      }
    };
    img.src = url;
    e.target.value = '';
  }

  // ── Import sprite sheet ───────────────────────────────────────────────────
  const sheetInputRef = useRef<HTMLInputElement>(null);
  const [sheetW, setSheetW] = useState('');
  const [sheetH, setSheetH] = useState('');

  function handleImportSheet(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file || selectedBank === null) return;
    const fw = parseInt(sheetW, 10);
    const fh = parseInt(sheetH, 10);
    if (isNaN(fw) || isNaN(fh) || fw <= 0 || fh <= 0) {
      setError('Set frame W and H before importing a sprite sheet.');
      return;
    }
    if (fw % 4 !== 0) { setError(`Frame width (${fw}) must be a multiple of 4.`); return; }
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => {
      URL.revokeObjectURL(url);
      const cols = Math.floor(img.width / fw);
      const rows = Math.floor(img.height / fh);
      if (cols === 0 || rows === 0) { setError('Image smaller than one frame.'); return; }
      const cvs = document.createElement('canvas');
      cvs.width = img.width;
      cvs.height = img.height;
      const ctx = cvs.getContext('2d');
      if (!ctx) return;
      ctx.drawImage(img, 0, 0);
      const newFrames: DecodedFrame[] = [];
      for (let row = 0; row < rows; row++) {
        for (let col = 0; col < cols; col++) {
          const imageData = ctx.getImageData(col * fw, row * fh, fw, fh);
          try {
            const indexed = quantizeToPalette(imageData, palette);
            newFrames.push({
              header: {
                width: fw,
                height: fh,
                offsetX: 0,
                offsetY: 0,
                compSize: 0,
                mode: 0,
                headerBytes: new Uint8Array(344),
              },
              indexedPixels: indexed,
              dirty: true,
            });
          } catch (err) {
            setError(err instanceof Error ? err.message : String(err));
            return;
          }
        }
      }
      setDecodedBanks(prev => {
        const m = new Map(prev);
        const bank = m.get(selectedBank);
        const frames = bank ? [...bank.frames, ...newFrames] : newFrames;
        m.set(selectedBank, { bankIndex: selectedBank, frames, dirty: true });
        return m;
      });
      const newCounts = [...(tabAssets?.frameCounts ?? new Array(256).fill(0))];
      newCounts[selectedBank] = (newCounts[selectedBank] ?? 0) + newFrames.length;
      setFolder(f => f && tabAssets ? { ...f, [tab]: { ...tabAssets, frameCounts: newCounts } } : f);
      setDirtyDat(true);
    };
    img.src = url;
    e.target.value = '';
  }

  // ── Reorder frame (drag-and-drop) ─────────────────────────────────────────
  function reorderFrame(bankIdx: number, fromIdx: number, toIdx: number) {
    if (fromIdx === toIdx) return;
    setDecodedBanks(prev => {
      const m = new Map(prev);
      const bank = m.get(bankIdx);
      if (!bank) return prev;
      const frames = [...bank.frames];
      const [moved] = frames.splice(fromIdx, 1);
      frames.splice(toIdx, 0, moved);
      m.set(bankIdx, { ...bank, frames, dirty: true });
      return m;
    });
    setSelectedFrame(toIdx);
  }

  // ── Zip export (all modified banks + DAT) ────────────────────────────────
  function handleDownloadZip() {
    if (!tabAssets) return;

    // Build updated DAT
    let datBuf: ArrayBuffer = tabAssets.datBuf.slice(0);
    for (const [idx, bank] of decodedBanks) {
      const p = encodeDat(datBuf, idx, bank.frames.length);
      datBuf = new ArrayBuffer(p.byteLength);
      new Uint8Array(datBuf).set(p);
    }
    for (const idx of deletedBanks) {
      const p = encodeDat(datBuf, idx, 0);
      datBuf = new ArrayBuffer(p.byteLength);
      new Uint8Array(datBuf).set(p);
    }

    const dir = DIR_NAME[tab];
    const files: Record<string, Uint8Array> = {
      [DAT_NAME[tab]]: new Uint8Array(datBuf),
    };

    for (const [idx, bank] of decodedBanks) {
      if (!bank.dirty) continue;
      const encoded = tab === 'tiles' ? encodeTileBank(bank) : encodeBank(bank);
      files[`${dir}/${bankFilename(tab, idx)}`] = encoded;
    }

    const zipped = zipSync(files, { level: 0 });
    const blob = new Blob([zipped.buffer as ArrayBuffer], { type: 'application/zip' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `silencer-${dir}.zip`;
    a.click();
    URL.revokeObjectURL(url);
  }

  // ── Export sprite sheet PNG ───────────────────────────────────────────────
  function handleExportSheet() {
    if (!currentBank || !palette) return;
    const frames = currentBank.frames;
    if (frames.length === 0) return;

    const cols = Math.ceil(Math.sqrt(frames.length));
    const rows = Math.ceil(frames.length / cols);
    const cellW = Math.max(...frames.map(f => f.header.width));
    const cellH = Math.max(...frames.map(f => f.header.height));

    const canvas = document.createElement('canvas');
    canvas.width  = cols * cellW;
    canvas.height = rows * cellH;
    const ctx = canvas.getContext('2d')!;
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    frames.forEach((frame, i) => {
      const col = i % cols;
      const row = Math.floor(i / cols);
      const imgData = frameToImageData(frame, palette);
      const tmp = document.createElement('canvas');
      tmp.width  = frame.header.width;
      tmp.height = frame.header.height;
      tmp.getContext('2d')!.putImageData(imgData, 0, 0);
      ctx.drawImage(tmp, col * cellW, row * cellH);
    });

    canvas.toBlob(blob => {
      if (!blob) return;
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `${BIN_PREFIX[tab]}${String(selectedBank!).padStart(3, '0')}_sheet.png`;
      a.click();
      URL.revokeObjectURL(url);
    }, 'image/png');
  }

  // ── Download .BIN ─────────────────────────────────────────────────────────
  function handleDownloadBin() {
    if (selectedBank === null || !currentBank) return;
    const bytes = tab === 'tiles' ? encodeTileBank(currentBank) : encodeBank(currentBank);
    const binBuf = new ArrayBuffer(bytes.byteLength);
    new Uint8Array(binBuf).set(bytes);
    const blob = new Blob([binBuf], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = bankFilename(tab, selectedBank);
    a.click();
    URL.revokeObjectURL(url);
  }

  // ── Download DAT ──────────────────────────────────────────────────────────
  function handleDownloadDat() {
    if (!folder || !tabAssets) return;
    let outBuf: ArrayBuffer = tabAssets.datBuf.slice(0);
    for (const [idx, bank] of decodedBanks) {
      const patched = encodeDat(outBuf, idx, bank.frames.length);
      outBuf = new ArrayBuffer(patched.byteLength);
      new Uint8Array(outBuf).set(patched);
    }
    for (const idx of deletedBanks) {
      const patched = encodeDat(outBuf, idx, 0);
      outBuf = new ArrayBuffer(patched.byteLength);
      new Uint8Array(outBuf).set(patched);
    }
    const blob = new Blob([outBuf], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = DAT_NAME[tab];
    a.click();
    URL.revokeObjectURL(url);
  }

  // ── Render ────────────────────────────────────────────────────────────────
  return (
    <div className="flex h-screen overflow-hidden bg-[#050a05] text-[#d1fad7]">
      <Sidebar wsConnected={wsConnected} />

      {/* Hidden file inputs */}
      <input
        ref={folderInputRef}
        type="file"
        // @ts-expect-error non-standard attribute
        webkitdirectory=""
        multiple
        className="hidden"
        onChange={handleFolderInput}
      />
      <input ref={pngInputRef} type="file" accept="image/png" className="hidden" onChange={handleImportPng} />
      <input ref={sheetInputRef} type="file" accept="image/png" className="hidden" onChange={handleImportSheet} />

      <main className="flex-1 flex flex-col min-w-0">
        {/* ── Header ── */}
        <div className="border-b border-[#1a2e1a] px-6 py-4 flex items-center justify-between">
          <div className="flex items-center gap-4">
            <span className="font-mono text-lg text-[#00a328]">◈ SPRITE BANK MANAGER</span>
            {folderName && (
              <span className="font-mono text-xs text-[#4a7a4a]">[ {folderName} ]</span>
            )}
          </div>
          <div className="flex items-center gap-2">
            {!folder ? (
              <button
                onClick={() => folderInputRef.current?.click()}
                className="px-3 py-1.5 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors"
              >
                [ OPEN FOLDER ]
              </button>
            ) : (
              <button
                onClick={handleCloseFolder}
                className="px-3 py-1.5 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#c94b4b] hover:text-[#c94b4b] transition-colors"
              >
                [ CLOSE ]
              </button>
            )}
          </div>
        </div>

        {/* ── Tabs ── */}
        <div className="border-b border-[#1a2e1a] px-6 flex gap-0">
          {TABS.map(t => (
            <button
              key={t}
              onClick={() => switchTab(t)}
              className={`px-5 py-2 text-xs font-mono tracking-widest border-b-2 transition-colors ${
                tab === t
                  ? 'border-[#00a328] text-[#00a328]'
                  : 'border-transparent text-[#4a7a4a] hover:text-[#d1fad7]'
              }`}
            >
              {t.toUpperCase()}
            </button>
          ))}
        </div>

        {/* ── Deleted bank notice ── */}
        {notice && (
          <div className="mx-4 mt-3 px-4 py-2 bg-[#2a1f00] border border-[#f59e0b] rounded font-mono text-xs text-[#f59e0b] flex items-start gap-3">
            <span>⚠</span>
            <div className="flex-1">{notice}</div>
            <button onClick={() => setNotice('')} className="text-[#f59e0b] hover:text-white">✕</button>
          </div>
        )}

        {/* ── Error ── */}
        {error && (
          <div className="mx-4 mt-3 px-4 py-2 bg-[#1a0808] border border-[#c94b4b] rounded font-mono text-xs text-[#c94b4b] flex items-start gap-3">
            <span>✕</span>
            <div className="flex-1">{error}</div>
            <button onClick={() => setError('')} className="hover:text-white">✕</button>
          </div>
        )}

        {!folder ? (
          <div className="flex-1 flex items-center justify-center">
            <div className="text-center flex flex-col items-center gap-6">
              <div className="text-6xl opacity-60">◈</div>
              <div>
                <p className="text-[#d1fad7] text-lg font-bold tracking-wide font-mono mb-1">SPRITE BANK MANAGER</p>
                <p className="text-[#4a7a4a] text-sm font-mono">Open <code className="text-[#00a328]">shared/assets/</code> to begin</p>
              </div>
              <div className="text-xs text-[#4a7a4a] font-mono space-y-1 border border-[#1a2e1a] p-4 text-left">
                <p>✦ View and manage sprite &amp; tile banks</p>
                <p>✦ Import PNG frames from external tools</p>
                <p>✦ Export .BIN banks and updated .DAT index</p>
                <p>✦ Add, delete, and reorder frames per bank</p>
              </div>
              <button
                onClick={() => folderInputRef.current?.click()}
                className="px-6 py-2 text-sm font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors"
              >
                [ OPEN FOLDER ]
              </button>
            </div>
          </div>
        ) : (
          <div className="flex flex-1 min-h-0">
            {/* ── Left panel: bank list ── */}
            <div
              className="flex flex-col border-r border-[#1a2e1a] min-h-0"
              style={{ width: 200, minWidth: 200 }}
            >
              {/* Toolbar */}
              <div className="flex gap-1 p-2 border-b border-[#1a2e1a]">
                <button
                  onClick={handleNewBank}
                  title="New Bank"
                  className="flex-1 px-2 py-1 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors"
                >
                  + BANK
                </button>
                <button
                  onClick={handleDeleteBank}
                  disabled={selectedBank === null}
                  title="Delete Bank"
                  className="flex-1 px-2 py-1 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#c94b4b] hover:text-[#c94b4b] transition-colors disabled:opacity-30"
                >
                  – BANK
                </button>
              </div>
              <div className="overflow-y-auto flex-1">
                {occupiedBanks.length === 0 && (
                  <div className="p-3 text-[#2a4a2a] text-xs font-mono">No banks found</div>
                )}
                {occupiedBanks.map(({ idx, count }) => {
                  const decoded = decodedBanks.get(idx);
                  const actualCount = decoded ? decoded.frames.length : count;
                  const isDirty = decoded?.dirty ?? false;
                  return (
                    <button
                      key={idx}
                      ref={el => { if (selectedBank === idx && el) el.scrollIntoView({ block: 'nearest' }); }}
                      onClick={() => selectBank(idx)}
                      className={`w-full text-left px-3 py-2 text-xs font-mono flex items-center gap-2 transition-colors ${
                        selectedBank === idx
                          ? 'bg-[#0a1a0a] text-[#00a328]'
                          : 'text-[#7aaa7a] hover:bg-[#080f08]'
                      }`}
                    >
                      {isDirty && (
                        <span className="w-1.5 h-1.5 rounded-full bg-[#f59e0b] flex-shrink-0" />
                      )}
                      <span className="flex-1">
                        {String(idx).padStart(3, '0')} · {actualCount}f
                      </span>
                    </button>
                  );
                })}
              </div>
            </div>

            {/* ── Center panel: frame thumbnails ── */}
            <div className="flex flex-col flex-1 min-w-0 border-r border-[#1a2e1a]">
              {/* Download toolbar */}
              <div className="flex items-center gap-2 px-3 py-2 border-b border-[#1a2e1a]">
                <button
                  onClick={handleDownloadBin}
                  disabled={!currentBank}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors disabled:opacity-30"
                >
                  ↓ .BIN
                </button>
                <button
                  onClick={handleDownloadDat}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors"
                >
                  ↓ {DAT_NAME[tab]}
                </button>
                <button
                  onClick={handleExportSheet}
                  disabled={!currentBank || !palette || currentBank.frames.length === 0}
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors disabled:opacity-30"
                >
                  ↓ SHEET.PNG
                </button>
                <button
                  onClick={handleDownloadZip}
                  disabled={!tabAssets}
                  title="Download all modified banks + index as a zip"
                  className="px-3 py-1 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors disabled:opacity-30"
                >
                  ↓ ZIP ALL
                </button>
                {selectedBank !== null && currentBank && (
                  <span className="ml-2 text-xs font-mono text-[#4a7a4a]">
                    Bank {String(selectedBank).padStart(3, '0')} — {currentBank.frames.length} frames
                    {currentBank.dirty && <span className="ml-2 text-[#f59e0b]">● modified</span>}
                  </span>
                )}
              </div>

              {/* Thumbnail grid */}
              <div className="flex-1 overflow-y-auto p-3">
                {selectedBank === null && (
                  <div className="text-[#2a4a2a] text-xs font-mono">Select a bank to view frames.</div>
                )}
                {selectedBank !== null && !currentBank && (
                  <div className="text-[#4a7a4a] text-xs font-mono">Loading…</div>
                )}
                {currentBank && (
                  <div className="flex flex-wrap gap-2">
                    {currentBank.frames.map((f, i) => (
                      <div
                        key={i}
                        draggable
                        onDragStart={() => setDraggingFrameIdx(i)}
                        onDragOver={e => e.preventDefault()}
                        onDrop={() => {
                          if (draggingFrameIdx !== null && selectedBank !== null) {
                            reorderFrame(selectedBank, draggingFrameIdx, i);
                          }
                          setDraggingFrameIdx(null);
                        }}
                        onDragEnd={() => setDraggingFrameIdx(null)}
                        className={`cursor-grab active:cursor-grabbing transition-opacity ${draggingFrameIdx === i ? 'opacity-40' : ''}`}
                      >
                        <Thumbnail
                          frame={f}
                          palette={palette}
                          selected={selectedFrame === i}
                          onClick={() => setSelectedFrame(i)}
                        />
                      </div>
                    ))}
                    {currentBank.frames.length === 0 && (
                      <div className="text-[#2a4a2a] text-xs font-mono">No frames. Import a PNG to add one.</div>
                    )}
                  </div>
                )}
              </div>
            </div>

            {/* ── Right panel: frame detail ── */}
            <div
              className="flex flex-col border-[#1a2e1a] overflow-y-auto"
              style={{ width: 320, minWidth: 320 }}
            >
              <div className="px-4 py-3 border-b border-[#1a2e1a]">
                <span className="text-xs font-mono text-[#4a7a4a]">FRAME DETAIL</span>
              </div>

              {/* Sub-palette selector */}
              <div className="px-4 py-2 border-b border-[#1a2e1a] flex items-center gap-2">
                <label className="text-xs font-mono text-[#7aaa7a]">SUB-PALETTE</label>
                <select
                  value={subPalette}
                  onChange={e => setSubPalette(parseInt(e.target.value, 10))}
                  className="ml-auto bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-0.5 rounded"
                >
                  {Array.from({ length: 11 }, (_, i) => (
                    <option key={i} value={i}>{i}</option>
                  ))}
                </select>
              </div>

              {/* Import controls */}
              <div className="px-4 py-3 border-b border-[#1a2e1a] flex flex-col gap-2">
                <button
                  onClick={() => {
                    if (selectedBank === null) { setError('Select a bank first.'); return; }
                    pngInputRef.current?.click();
                  }}
                  className="w-full px-3 py-1.5 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors text-left"
                >
                  + IMPORT PNG
                </button>
                <div className="flex gap-2 items-center">
                  <input
                    type="number"
                    placeholder="W"
                    value={sheetW}
                    onChange={e => setSheetW(e.target.value)}
                    className="w-14 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded"
                  />
                  <span className="text-[#4a7a4a] text-xs">×</span>
                  <input
                    type="number"
                    placeholder="H"
                    value={sheetH}
                    onChange={e => setSheetH(e.target.value)}
                    className="w-14 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded"
                  />
                  <button
                    onClick={() => {
                      if (selectedBank === null) { setError('Select a bank first.'); return; }
                      sheetInputRef.current?.click();
                    }}
                    className="flex-1 px-2 py-1 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#00a328] hover:text-[#00a328] transition-colors"
                  >
                    + SHEET
                  </button>
                </div>
              </div>

              {currentFrame ? (
                <div className="flex flex-col gap-4 p-4">
                  {/* Canvas preview */}
                  <FrameCanvas frame={currentFrame} palette={palette} />

                  {/* Dimensions */}
                  <div className="font-mono text-xs text-[#7aaa7a]">
                    {currentFrame.header.width} × {currentFrame.header.height} px
                    {currentFrame.dirty && <span className="ml-2 text-[#f59e0b]">● dirty</span>}
                  </div>

                  {/* Anchor controls */}
                  <div className="flex flex-col gap-2">
                    <div className="text-xs font-mono text-[#4a7a4a]">ANCHOR OFFSET</div>
                    <div className="flex gap-3 items-center">
                      <label className="text-xs font-mono text-[#7aaa7a] w-8">X</label>
                      <input
                        type="number"
                        value={currentFrame.header.offsetX}
                        onChange={e => {
                          if (selectedBank === null || selectedFrame === null) return;
                          patchFrameHeader(selectedBank, selectedFrame, {
                            offsetX: parseInt(e.target.value, 10) || 0,
                          });
                        }}
                        className="w-20 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded"
                      />
                    </div>
                    <div className="flex gap-3 items-center">
                      <label className="text-xs font-mono text-[#7aaa7a] w-8">Y</label>
                      <input
                        type="number"
                        value={currentFrame.header.offsetY}
                        onChange={e => {
                          if (selectedBank === null || selectedFrame === null) return;
                          patchFrameHeader(selectedBank, selectedFrame, {
                            offsetY: parseInt(e.target.value, 10) || 0,
                          });
                        }}
                        className="w-20 bg-[#080f08] border border-[#1a2e1a] text-[#d1fad7] text-xs font-mono px-2 py-1 rounded"
                      />
                    </div>
                  </div>

                  {/* Mode */}
                  <div className="text-xs font-mono text-[#4a7a4a]">
                    MODE: {currentFrame.header.mode === 0 ? 'LINEAR RLE' : `TILE (${currentFrame.header.mode})`}
                  </div>

                  {/* Delete frame */}
                  <button
                    onClick={() => {
                      if (selectedBank === null || selectedFrame === null) return;
                      deleteFrame(selectedBank, selectedFrame);
                    }}
                    className="w-full mt-2 px-3 py-1.5 text-xs font-mono border border-[#1a2e1a] rounded hover:border-[#c94b4b] hover:text-[#c94b4b] transition-colors"
                  >
                    – DELETE FRAME
                  </button>
                </div>
              ) : (
                <div className="p-4 text-[#2a4a2a] text-xs font-mono">
                  Select a frame to view details.
                </div>
              )}
            </div>
          </div>
        )}
      </main>
    </div>
  );
}

// ── Page export with Suspense (Next.js 14 requirement for useSearchParams) ────

export default function SpritesPage() {
  return (
    <Suspense>
      <SpritesPageInner />
    </Suspense>
  );
}
