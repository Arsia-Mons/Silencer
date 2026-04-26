'use client';
import { useEffect, useRef } from 'react';
import type { TileCell } from '../../lib/types';

export interface TileMenuInfo {
  x: number;
  y: number;
  tx: number;
  ty: number;
  layerType: string;
  layerIdx: number;
  cell: TileCell | null;
}

interface Props {
  menu: TileMenuInfo | null;
  onPatch: (patch: Partial<TileCell>) => void;
  onClear: () => void;
  onClose: () => void;
}

export default function TileContextMenu({ menu, onPatch, onClear, onClose }: Props) {
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) onClose();
    };
    const keyHandler = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose(); };
    document.addEventListener('mousedown', handler);
    document.addEventListener('keydown', keyHandler);
    return () => {
      document.removeEventListener('mousedown', handler);
      document.removeEventListener('keydown', keyHandler);
    };
  }, [onClose]);

  if (!menu) return null;
  const { x, y, tx, ty, layerType, layerIdx, cell } = menu;
  const { tile_id = 0, flip = 0, lum = 0 } = cell ?? {};

  const style: React.CSSProperties = {
    position: 'fixed',
    left: Math.min(x, window.innerWidth - 210),
    top: Math.min(y, window.innerHeight - 180),
    zIndex: 9999,
  };

  const Row = ({ label, active, onClick }: { label: string; active: boolean; onClick: () => void }) => (
    <button
      onClick={onClick}
      className={`flex items-center gap-2 w-full px-3 py-1.5 text-xs text-left hover:bg-[#1a2e1a] transition-colors ${active ? 'text-[#80ff80]' : 'text-[#809080]'}`}
    >
      <span className={`w-3 h-3 rounded-sm border ${active ? 'bg-[#80ff80] border-[#80ff80]' : 'border-[#3a4a3a]'}`} />
      {label}
    </button>
  );

  return (
    <div ref={ref} style={style}
      className="bg-[#0d150d] border border-[#2a3a2a] rounded shadow-xl w-52 py-1 font-mono"
    >
      <div className="px-3 py-1 text-[10px] text-[#3a6a3a] border-b border-[#1a2a1a] mb-1">
        Tile ({tx},{ty}) · {layerType.toUpperCase()}[{layerIdx}]
        {tile_id ? ` · id:${tile_id}` : ' · empty'}
      </div>

      <Row
        label="Flip X"
        active={!!flip}
        onClick={() => { onPatch({ flip: flip ? 0 : 1 }); onClose(); }}
      />
      <Row
        label="Luminous (LUM)"
        active={!!lum}
        onClick={() => { onPatch({ lum: lum ? 0 : 255 }); onClose(); }}
      />

      <div className="border-t border-[#1a2a1a] mt-1 pt-1">
        <button
          onClick={() => { onClear(); onClose(); }}
          disabled={!tile_id}
          className="flex items-center gap-2 w-full px-3 py-1.5 text-xs text-left text-[#ff6060] hover:bg-[#2a1a1a] disabled:opacity-30 disabled:cursor-not-allowed transition-colors"
        >
          Clear tile
        </button>
      </div>
    </div>
  );
}
