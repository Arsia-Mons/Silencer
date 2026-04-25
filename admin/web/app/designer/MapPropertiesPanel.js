'use client';
import { useState, useEffect } from 'react';

export default function MapPropertiesPanel({ header, onUpdate, onClose }) {
  const [fields, setFields] = useState({
    description: header?.description ?? '',
    ambience:    String(header?.ambience   ?? 0),
    parallax:    String(header?.parallax   ?? 0),
    maxplayers:  String(header?.maxplayers ?? 8),
    maxteams:    String(header?.maxteams   ?? 2),
  });

  useEffect(() => {
    setFields({
      description: header?.description ?? '',
      ambience:    String(header?.ambience   ?? 0),
      parallax:    String(header?.parallax   ?? 0),
      maxplayers:  String(header?.maxplayers ?? 8),
      maxteams:    String(header?.maxteams   ?? 2),
    });
  }, [header]);

  const apply = () => {
    onUpdate({
      description: fields.description.slice(0, 127),
      ambience:    Math.max(-128, Math.min(127, parseInt(fields.ambience,   10) || 0)),
      parallax:    Math.max(0,    Math.min(255, parseInt(fields.parallax,   10) || 0)),
      maxplayers:  Math.max(1,    Math.min(8,   parseInt(fields.maxplayers, 10) || 8)),
      maxteams:    Math.max(1,    Math.min(4,   parseInt(fields.maxteams,   10) || 2)),
    });
  };

  const inp = 'w-full bg-game-dark border border-game-border text-game-text text-xs px-2 py-1 rounded font-mono focus:outline-none focus:border-game-primary';
  const lbl = 'text-game-textDim text-xs font-mono mb-0.5';

  return (
    <div className="bg-game-bgCard border-b border-game-border px-4 py-3 flex flex-wrap gap-4 items-end">
      <div className="flex-1 min-w-[180px]">
        <div className={lbl}>Description (max 127)</div>
        <input type="text" maxLength={127} value={fields.description}
          onChange={e => setFields(f => ({ ...f, description: e.target.value }))}
          className={inp} />
      </div>
      <div className="w-28">
        <div className={lbl}>Ambience (-128..127)</div>
        <input type="number" min={-128} max={127} value={fields.ambience}
          onChange={e => setFields(f => ({ ...f, ambience: e.target.value }))}
          className={inp} />
      </div>
      <div className="w-28">
        <div className={lbl}>Parallax (0..255)</div>
        <input type="number" min={0} max={255} value={fields.parallax}
          onChange={e => setFields(f => ({ ...f, parallax: e.target.value }))}
          className={inp} />
      </div>
      <div className="w-24">
        <div className={lbl}>Max Players</div>
        <input type="number" min={1} max={8} value={fields.maxplayers}
          onChange={e => setFields(f => ({ ...f, maxplayers: e.target.value }))}
          className={inp} />
      </div>
      <div className="w-24">
        <div className={lbl}>Max Teams</div>
        <input type="number" min={1} max={4} value={fields.maxteams}
          onChange={e => setFields(f => ({ ...f, maxteams: e.target.value }))}
          className={inp} />
      </div>
      <div className="flex gap-2 flex-shrink-0">
        <button onClick={apply}
          className="px-3 py-1 text-xs font-mono border border-game-primary text-game-primary rounded hover:bg-game-dark transition-colors">
          APPLY
        </button>
        <button onClick={onClose}
          className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary transition-colors">
          ✕
        </button>
      </div>
    </div>
  );
}
