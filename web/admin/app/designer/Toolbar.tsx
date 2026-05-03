'use client';

export interface ToolDef {
  id: string;
  label: string;
  icon: string;
}

export interface PlatformToolType {
  type1: number;
  type2: number;
  typeName: string;
}

export interface ActorDefEntry {
  id: number;
  label: string;
  icon: string;
  color: string;
  bank: number | null;
  frame: number;
}

export interface ActorTypeHint {
  label: string;
  options: Record<string | number, string>;
}

export const TOOLS: ToolDef[] = [
  { id: 'SELECT',         label: 'SELECT',     icon: '⊹' },
  { id: 'TILE_BG',        label: 'TILE (BG)',  icon: '▦' },
  { id: 'TILE_FG',        label: 'TILE (FG)',  icon: '▧' },
  { id: 'ERASE_TILE',     label: 'ERASE',      icon: '⌫' },
  { id: 'TILE_SELECT',    label: 'COPY SEL',   icon: '⬚' },
  { id: 'FLOOD_FILL',    label: 'FILL',        icon: '🪣' },
  { id: 'RECT',           label: 'RECT',       icon: '▭' },
  { id: 'STAIRSUP',       label: 'STAIRS↑',   icon: '↗' },
  { id: 'STAIRSDOWN',     label: 'STAIRS↓',   icon: '↘' },
  { id: 'LADDER',         label: 'LADDER',     icon: '⌇' },
  { id: 'TRACK',          label: 'TRACK',      icon: '⟺' },
  { id: 'OUTSIDEROOM',    label: 'RAIN',       icon: '🌧' },
  { id: 'SPECIFICROOM',   label: 'ROOM',       icon: '▣' },
  { id: 'ERASE_PLATFORM', label: 'ERASE PLT',  icon: '✕' },
  { id: 'ACTOR',          label: 'ACTOR',      icon: '☻' },
  { id: 'SHADOW_ZONE',    label: 'SHADOW',     icon: '🌑' },
  { id: 'NAV_LINK',       label: 'LINK',       icon: '⇒' },
];

export const PLATFORM_TOOL_TYPES: Record<string, PlatformToolType> = {
  RECT:         { type1: 0, type2: 0, typeName: 'RECTANGLE' },
  STAIRSUP:     { type1: 0, type2: 1, typeName: 'STAIRSUP' },
  STAIRSDOWN:   { type1: 0, type2: 2, typeName: 'STAIRSDOWN' },
  LADDER:       { type1: 1, type2: 0, typeName: 'LADDER' },
  TRACK:        { type1: 2, type2: 0, typeName: 'TRACK' },
  OUTSIDEROOM:  { type1: 3, type2: 0, typeName: 'OUTSIDEROOM' },
  SPECIFICROOM: { type1: 3, type2: 1, typeName: 'SPECIFICROOM' },
};

export const ACTOR_DEFS: ActorDefEntry[] = [
  { id: 0,  label: 'Guard Blaster',   icon: 'GB', color: '#ef4444', bank: 59,   frame: 0 },
  { id: 1,  label: 'Civilian',        icon: 'CV', color: '#f59e0b', bank: 121,  frame: 0 },
  { id: 2,  label: 'Captain Laser',   icon: 'CL', color: '#ef4444', bank: 59,   frame: 0 },
  { id: 3,  label: 'Trooper Rocket',  icon: 'TR', color: '#ef4444', bank: 59,   frame: 0 },
  { id: 6,  label: 'Robot',           icon: 'RB', color: '#a855f7', bank: 47,   frame: 0 },
  { id: 36, label: 'Player Start',    icon: 'PS', color: '#22d3ee', bank: 78,   frame: 0 },
  { id: 37, label: 'Camera',          icon: 'CM', color: '#f59e0b', bank: 65,   frame: 0 },
  { id: 47, label: 'Doodad',          icon: 'DD', color: '#6b7280', bank: null, frame: 0 },
  { id: 50, label: 'Surv. Monitor',   icon: 'SM', color: '#f59e0b', bank: 65,   frame: 0 },
  { id: 54, label: 'Terminal',        icon: 'TM', color: '#00a328', bank: 183,  frame: 0 },
  { id: 56, label: 'Inv. Station',    icon: 'IS', color: '#00a328', bank: 89,   frame: 0 },
  { id: 57, label: 'Heal Machine',    icon: 'HM', color: '#22d3ee', bank: 172,  frame: 0 },
  { id: 58, label: 'Secret Return',   icon: 'SR', color: '#a855f7', bank: 152,  frame: 0 },
  { id: 61, label: 'Warper',          icon: 'WP', color: '#a855f7', bank: 85,   frame: 0 },
  { id: 63, label: 'Powerup',         icon: 'PU', color: '#f59e0b', bank: null, frame: 0 },
  { id: 64, label: 'Vent',            icon: 'VT', color: '#6b7280', bank: 179,  frame: 0 },
  { id: 65, label: 'Base Exit',       icon: 'BE', color: '#22d3ee', bank: 101,  frame: 0 },
  { id: 66, label: 'Tech Station',    icon: 'TS', color: '#00a328', bank: 106,  frame: 0 },
  { id: 67, label: 'Laser Defense',   icon: 'LD', color: '#ef4444', bank: 112,  frame: 0 },
  { id: 68, label: 'Team Billboard',  icon: 'TB', color: '#22d3ee', bank: 151,  frame: 0 },
  { id: 69, label: 'Computer',        icon: 'PC', color: '#6b7280', bank: 171,  frame: 0 },
  { id: 70, label: 'Credit Machine',  icon: 'CR', color: '#f59e0b', bank: 80,   frame: 0 },
  { id: 71, label: 'Light',           icon: 'LT', color: '#fde68a', bank: 222,  frame: 0 },
];

export const ACTOR_TYPE_HINTS: Record<number, ActorTypeHint> = {
  0:  { label: 'Behavior', options: { 0:'Patrol', 1:'Guard (stationary)' } },
  1:  { label: 'Variant',  options: { 0:'Civilian A', 1:'Civilian B' } },
  2:  { label: 'Behavior', options: { 0:'Patrol', 1:'Guard (stationary)' } },
  3:  { label: 'Behavior', options: { 0:'Patrol', 1:'Guard (stationary)', 2:"Magistrate's Laser", 3:"Magistrate's Rocket" } },
  6:  { label: 'Behavior', options: { 0:'Patrol', 1:'Guard (stationary)' } },
  54: { label: 'Size',     options: { 0:'Small', 1:'Big' } },
  66: { label: 'Variant',  options: { 0:'Type A', 1:'Type B', 2:'Type C (surveillance-linked)' } },
  63: { label: 'Powerup',  options: {
    0:'Super Shield', 1:'Neutron Bomb', 2:'Jet Pack',
    3:'Invisible', 4:'Hacking Bonus', 5:'Radar', 6:'Depositor',
  } },
  47: { label: 'Doodad',   options: {
    0:'Small Candle', 1:'Large Candle', 2:'Small Canister', 3:'Large Canister',
    4:'Arrow Poster', 5:'Man in Tank', 6:'Doodad 6', 7:'Doodad 7', 8:'Doodad 8', 9:'Doodad 9',
  } },
  50: { label: 'Size',     options: { 4:'Small', 5:'Small Alt', 6:'Large', 7:'Default' } },
  65: { label: 'Side',     options: { 0:'Team A', 1:'Team B' } },
  67: { label: 'Type',     options: { 0:'Base Defense', 1:'Guard Defense (Laser)' } },
};

interface ToolbarProps {
  activeTool: string;
  onToolChange: (tool: string) => void;
  activeLayer: number;
  onLayerChange: (layer: number) => void;
  selectedActor: number;
  onActorChange: (id: number) => void;
  lumMode: boolean;
  onLumModeChange: (v: boolean) => void;
  eraseLayerType: string;
  onEraseLayerTypeChange?: (t: string) => void;
  navLinkType?: 0 | 1 | 2;
  onNavLinkTypeChange?: (t: 0 | 1 | 2) => void;
}

export default function Toolbar({
  activeTool, onToolChange, activeLayer, onLayerChange,
  selectedActor, onActorChange, lumMode, onLumModeChange,
  eraseLayerType, onEraseLayerTypeChange,
  navLinkType, onNavLinkTypeChange,
}: ToolbarProps) {
  const tileTools     = TOOLS.filter(t => ['TILE_BG', 'TILE_FG', 'ERASE_TILE', 'TILE_SELECT', 'FLOOD_FILL'].includes(t.id));
  const platformTools = TOOLS.filter(t => ['RECT','STAIRSUP','STAIRSDOWN','LADDER','TRACK','OUTSIDEROOM','SPECIFICROOM','ERASE_PLATFORM'].includes(t.id));
  const otherTools    = TOOLS.filter(t => ['SELECT','ACTOR','SHADOW_ZONE','NAV_LINK'].includes(t.id));

  const btnCls = (id: string) =>
    `px-2 py-1 text-xs font-mono border rounded transition-colors ${
      activeTool === id
        ? 'border-game-primary text-game-primary bg-game-dark'
        : 'border-game-border text-game-textDim hover:border-game-primary hover:text-game-text'
    }`;

  const layerCls = (l: number) =>
    `px-2 py-1 text-xs font-mono border rounded transition-colors ${
      activeLayer === l
        ? 'border-game-primary text-game-primary bg-game-dark'
        : 'border-game-border text-game-textDim hover:border-game-primary'
    }`;

  return (
    <div className="flex items-center gap-3 px-3 py-2 bg-game-bgCard border-b border-game-border flex-wrap">
      <div className="flex gap-1">
        {otherTools.map(t => (
          <button key={t.id} onClick={() => onToolChange(t.id)} className={btnCls(t.id)} title={t.label}>
            {t.icon} {t.label}
          </button>
        ))}
      </div>

      <div className="w-px h-5 bg-game-border" />

      <div className="flex gap-1">
        {tileTools.map(t => (
          <button key={t.id} onClick={() => onToolChange(t.id)} className={btnCls(t.id)} title={t.label}>
            {t.icon} {t.label}
          </button>
        ))}
      </div>

      {(activeTool === 'TILE_BG' || activeTool === 'TILE_FG') && (
        <>
          <div className="w-px h-5 bg-game-border" />
          <div className="flex gap-1 items-center">
            <span className="text-xs text-game-textDim mr-1">LAYER:</span>
            {[0,1,2,3].map(l => (
              <button key={l} onClick={() => onLayerChange(l)} className={layerCls(l)}>{l}</button>
            ))}
          </div>
          <div className="w-px h-5 bg-game-border" />
          <button
            onClick={() => onLumModeChange(!lumMode)}
            className={`px-2 py-1 text-xs font-mono border rounded transition-colors ${
              lumMode
                ? 'border-yellow-400 text-yellow-300 bg-game-dark'
                : 'border-game-border text-game-textDim hover:border-game-primary'
            }`}
            title="Toggle LUM flag — lit tiles ignore ambient darkness"
          >
            💡 LUM
          </button>
        </>
      )}

      {activeTool === 'ERASE_TILE' && (
        <>
          <div className="w-px h-5 bg-game-border" />
          <div className="flex gap-1 items-center">
            <span className="text-xs text-game-textDim mr-1">TYPE:</span>
            {(['bg','fg'] as const).map(t => (
              <button key={t} onClick={() => onEraseLayerTypeChange?.(t)}
                className={`px-2 py-1 text-xs font-mono border rounded transition-colors ${
                  eraseLayerType === t
                    ? 'border-game-primary text-game-primary bg-game-dark'
                    : 'border-game-border text-game-textDim hover:border-game-primary'
                }`}>
                {t.toUpperCase()}
              </button>
            ))}
          </div>
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

      <div className="flex gap-1 flex-wrap">
        {platformTools.map(t => (
          <button key={t.id} onClick={() => onToolChange(t.id)} className={btnCls(t.id)} title={t.label}>
            {t.icon} {t.label}
          </button>
        ))}
      </div>

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
      {activeTool === 'NAV_LINK' && (
        <>
          <div className="w-px h-5 bg-game-border" />
          <div className="flex gap-1 items-center">
            <span className="text-xs text-game-textDim mr-1">TYPE:</span>
            {(['JUMP', 'FALL', 'JETPACK'] as const).map((label, i) => (
              <button key={label} onClick={() => onNavLinkTypeChange?.(i as 0 | 1 | 2)}
                className={`px-2 py-1 text-xs font-mono border rounded transition-colors ${
                  navLinkType === i
                    ? 'border-game-primary text-game-primary bg-game-dark'
                    : 'border-game-border text-game-textDim hover:border-game-primary'
                }`}>
                {label}
              </button>
            ))}
          </div>
        </>
      )}
    </div>
  );
}
