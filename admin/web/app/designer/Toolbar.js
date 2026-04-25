'use client';

export const TOOLS = [
  { id: 'SELECT',         label: 'SELECT',     icon: '⊹' },
  { id: 'TILE_BG',        label: 'TILE (BG)',  icon: '▦' },
  { id: 'TILE_FG',        label: 'TILE (FG)',  icon: '▧' },
  { id: 'RECT',           label: 'RECT',       icon: '▭' },
  { id: 'STAIRSUP',       label: 'STAIRS↑',   icon: '↗' },
  { id: 'STAIRSDOWN',     label: 'STAIRS↓',   icon: '↘' },
  { id: 'LADDER',         label: 'LADDER',     icon: '⌇' },
  { id: 'TRACK',          label: 'TRACK',      icon: '⟺' },
  { id: 'ERASE_PLATFORM', label: 'ERASE PLT',  icon: '✕' },
  { id: 'ACTOR',          label: 'ACTOR',      icon: '☻' },
];

export const PLATFORM_TOOL_TYPES = {
  RECT:       { type1: 0, type2: 0, typeName: 'RECTANGLE' },
  STAIRSUP:   { type1: 0, type2: 1, typeName: 'STAIRSUP' },
  STAIRSDOWN: { type1: 0, type2: 2, typeName: 'STAIRSDOWN' },
  LADDER:     { type1: 1, type2: 0, typeName: 'LADDER' },
  TRACK:      { type1: 2, type2: 0, typeName: 'TRACK' },
};

export const ACTOR_DEFS = [
  { id: 0,  label: 'Guard Blaster',     icon: 'GB', color: '#ef4444' },
  { id: 1,  label: 'Civilian',          icon: 'CV', color: '#f59e0b' },
  { id: 2,  label: 'Captain Laser',     icon: 'CL', color: '#ef4444' },
  { id: 3,  label: 'Trooper Rocket',    icon: 'TR', color: '#ef4444' },
  { id: 6,  label: 'Robot',             icon: 'RB', color: '#a855f7' },
  { id: 36, label: 'Player Start',      icon: 'PS', color: '#22d3ee' },
  { id: 37, label: 'Camera',            icon: 'CM', color: '#f59e0b' },
  { id: 54, label: 'Terminal',          icon: 'TM', color: '#00a328' },
  { id: 56, label: 'Inv. Station',      icon: 'IS', color: '#00a328' },
  { id: 57, label: 'Heal Machine',      icon: 'HM', color: '#22d3ee' },
  { id: 58, label: 'Secret Return',     icon: 'SR', color: '#a855f7' },
  { id: 63, label: 'Powerup',           icon: 'PU', color: '#f59e0b' },
  { id: 64, label: 'Vent',              icon: 'VT', color: '#6b7280' },
  { id: 65, label: 'Base Exit',         icon: 'BE', color: '#22d3ee' },
  { id: 66, label: 'Tech Station',      icon: 'TS', color: '#00a328' },
  { id: 70, label: 'Credit Machine',    icon: 'CR', color: '#f59e0b' },
];

export default function Toolbar({ activeTool, onToolChange, activeLayer, onLayerChange, selectedActor, onActorChange }) {
  const tileTools = TOOLS.filter(t => t.id === 'TILE_BG' || t.id === 'TILE_FG');
  const platformTools = TOOLS.filter(t => ['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK','ERASE_PLATFORM'].includes(t.id));
  const otherTools = TOOLS.filter(t => ['SELECT','ACTOR'].includes(t.id));

  const btnCls = (id) =>
    `px-2 py-1 text-xs font-mono border rounded transition-colors ${
      activeTool === id
        ? 'border-game-primary text-game-primary bg-game-dark'
        : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'
    }`;

  const layerCls = (l) =>
    `px-2 py-1 text-xs font-mono border rounded transition-colors ${
      activeLayer === l
        ? 'border-game-primary text-game-primary bg-game-dark'
        : 'border-game-border text-game-textDim hover:border-game-primary'
    }`;

  return (
    <div className="flex items-center gap-3 px-3 py-2 bg-game-bgCard border-b border-game-border flex-wrap">
      {/* Selection / Actor */}
      <div className="flex gap-1">
        {otherTools.map(t => (
          <button key={t.id} onClick={() => onToolChange(t.id)} className={btnCls(t.id)} title={t.label}>
            {t.icon} {t.label}
          </button>
        ))}
      </div>

      <div className="w-px h-5 bg-game-border" />

      {/* Tile tools */}
      <div className="flex gap-1">
        {tileTools.map(t => (
          <button key={t.id} onClick={() => onToolChange(t.id)} className={btnCls(t.id)} title={t.label}>
            {t.icon} {t.label}
          </button>
        ))}
      </div>

      {/* Layer selector (shown when tile tool active) */}
      {(activeTool === 'TILE_BG' || activeTool === 'TILE_FG') && (
        <>
          <div className="w-px h-5 bg-game-border" />
          <div className="flex gap-1 items-center">
            <span className="text-xs text-game-textDim mr-1">LAYER:</span>
            {[0,1,2,3].map(l => (
              <button key={l} onClick={() => onLayerChange(l)} className={layerCls(l)}>{l}</button>
            ))}
          </div>
        </>
      )}

      <div className="w-px h-5 bg-game-border" />

      {/* Platform tools */}
      <div className="flex gap-1 flex-wrap">
        {platformTools.map(t => (
          <button key={t.id} onClick={() => onToolChange(t.id)} className={btnCls(t.id)} title={t.label}>
            {t.icon} {t.label}
          </button>
        ))}
      </div>

      {/* Actor picker (shown when ACTOR tool active) */}
      {activeTool === 'ACTOR' && (
        <>
          <div className="w-px h-5 bg-game-border" />
          <select
            value={selectedActor}
            onChange={e => onActorChange(Number(e.target.value))}
            className="bg-game-bgCard border border-game-border text-game-text font-mono text-xs px-2 py-1 rounded focus:outline-none focus:border-game-primary"
          >
            {ACTOR_DEFS.map(a => (
              <option key={a.id} value={a.id}>{a.label} (id={a.id})</option>
            ))}
          </select>
        </>
      )}
    </div>
  );
}
