'use client';
import { useRef, useEffect, useState } from 'react';

const TILE_SIZE = 64;
const TILE_GAP  = 1;
const TILES_PER_ROW = 8;

function TileCell({ bitmap, selected, onClick }) {
  const ref = useRef(null);

  useEffect(() => {
    const canvas = ref.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, TILE_SIZE, TILE_SIZE);
    if (bitmap) {
      ctx.drawImage(bitmap, 0, 0, TILE_SIZE, TILE_SIZE);
    } else {
      ctx.fillStyle = '#0a0a0f';
      ctx.fillRect(0, 0, TILE_SIZE, TILE_SIZE);
    }
  }, [bitmap]);

  return (
    <canvas
      ref={ref}
      width={TILE_SIZE}
      height={TILE_SIZE}
      onClick={onClick}
      style={{
        width: TILE_SIZE,
        height: TILE_SIZE,
        cursor: 'pointer',
        border: selected ? '2px solid #00a328' : '1px solid #1a2e1a',
        boxSizing: 'border-box',
        background: '#0a0a0f',
        flexShrink: 0,
      }}
    />
  );
}

export default function TilePicker({ tileImages, tileBankCounts, selectedTileId, onSelectTile }) {
  const [bankNum, setBankNum] = useState(0);
  const [filter, setFilter] = useState('');

  const sortedBanks = Array.from(tileBankCounts.keys()).sort((a, b) => a - b);
  const currentBankIdx = sortedBanks.indexOf(bankNum);

  const goToBank = (n) => setBankNum(n);
  const prevBank = () => {
    const idx = currentBankIdx > 0 ? currentBankIdx - 1 : sortedBanks.length - 1;
    setBankNum(sortedBanks[idx] ?? 0);
  };
  const nextBank = () => {
    const idx = currentBankIdx < sortedBanks.length - 1 ? currentBankIdx + 1 : 0;
    setBankNum(sortedBanks[idx] ?? 0);
  };

  const handleFilterChange = (val) => {
    setFilter(val);
    if (!val.trim()) return;
    if (/^\d+:\d+$/.test(val.trim())) {
      const [b, t] = val.trim().split(':').map(Number);
      setBankNum(b);
      onSelectTile((b << 8) | t);
    } else if (/^\d+$/.test(val.trim())) {
      setBankNum(parseInt(val.trim(), 10));
    }
  };

  const bitmaps = tileImages.get(bankNum) ?? [];
  const count   = tileBankCounts.get(bankNum) ?? 0;

  const selectedBank = selectedTileId ? (selectedTileId >> 8) & 0xFF : -1;
  const selectedIdx  = selectedTileId ? selectedTileId & 0xFF : -1;

  return (
    <div className="flex flex-col h-full overflow-hidden">
      <div className="px-3 py-2 border-b border-game-border">
        <div className="text-xs text-game-textDim mb-2 tracking-widest">TILE BANK</div>
        <input
          type="text"
          placeholder="Filter: bank # or bank:tile"
          value={filter}
          onChange={e => handleFilterChange(e.target.value)}
          className="w-full mb-2 px-2 py-1 text-xs font-mono bg-game-dark border border-game-border text-game-text rounded focus:outline-none focus:border-game-primary placeholder:text-game-muted"
        />
        <div className="flex items-center gap-1 mb-2">
          <button onClick={prevBank} className="px-2 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text">◀</button>
          <input
            type="number"
            min={0}
            max={255}
            value={bankNum}
            onChange={e => goToBank(Math.max(0, Math.min(255, Number(e.target.value))))}
            className="w-14 text-center bg-game-bgCard border border-game-border text-game-text font-mono text-xs px-2 py-1 rounded focus:outline-none focus:border-game-primary"
          />
          <button onClick={nextBank} className="px-2 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary hover:text-game-text">▶</button>
          <span className="text-xs text-game-muted ml-1">{count} tiles</span>
        </div>
        {sortedBanks.length > 0 && (
          <div className="text-xs text-game-textDim">
            Bank {currentBankIdx + 1}/{sortedBanks.length} with tiles
          </div>
        )}
      </div>

      {selectedTileId ? (
        <div className="px-3 py-2 border-b border-game-border text-xs font-mono">
          <span className="text-game-textDim">SEL: </span>
          <span className="text-game-primary">bank {selectedBank} / idx {selectedIdx}</span>
          <span className="text-game-textDim ml-2">id=0x{selectedTileId.toString(16).toUpperCase().padStart(4,'0')}</span>
          <button
            onClick={() => onSelectTile(0)}
            className="ml-2 text-game-danger hover:text-red-400 text-xs"
            title="Clear selection"
          >✕</button>
        </div>
      ) : (
        <div className="px-3 py-2 border-b border-game-border text-xs text-game-muted">No tile selected</div>
      )}

      <div className="flex-1 overflow-y-auto p-2">
        {bitmaps.length === 0 && count === 0 && (
          <div className="text-xs text-game-muted text-center py-4">
            {tileImages.size === 0 ? 'Load TIL_XXX.BIN files' : 'No tiles in this bank'}
          </div>
        )}
        <div className="flex flex-wrap gap-px">
          {bitmaps.map((bmp, idx) => {
            const tileId = (bankNum << 8) | idx;
            return (
              <TileCell
                key={idx}
                bitmap={bmp}
                selected={selectedBank === bankNum && selectedIdx === idx}
                onClick={() => onSelectTile(tileId)}
              />
            );
          })}
        </div>
      </div>
    </div>
  );
}
