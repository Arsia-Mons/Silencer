import {
  ALIGNS,
  AXES,
  FONTS,
  JUSTIFIES,
  KIND_LABELS,
  SIZE_MODES,
} from '../ui-editor-constants';
import { slugify } from '../ui-editor-utils';
import {
  PALETTE_NODE_KINDS,
  canHaveChildren,
  type UiAlign,
  type UiAxis,
  type UiFont,
  type UiJustify,
  type UiNode,
  type UiNodeKind,
  type UiSize,
  type UiSizeMode,
  type UiStyle,
} from '../../../lib/ui-layout';
import { Field, NumberInput, Select, TextInput } from './EditorControls';

interface InspectorProps {
  node: UiNode;
  parent: UiNode | null;
  isRoot: boolean;
  onPatch: (patch: Partial<UiNode>) => void;
  onStyle: (style: Partial<UiStyle>) => void;
  onDelete: () => void;
  onDuplicate: () => void;
  onAddChild: (kind: UiNodeKind) => void;
}

export function Inspector({ node, parent, isRoot, onPatch, onStyle, onDelete, onDuplicate, onAddChild }: InspectorProps) {
  const supportsChildren = canHaveChildren(node.kind);
  return (
    <aside className="min-h-0 overflow-auto border-l border-game-border bg-game-bgCard/95">
      <div className="p-4 border-b border-game-border">
        <div className="text-xs tracking-widest text-game-primary mb-3">INSPECTOR</div>
        <div className="text-[11px] tracking-widest text-game-textDim">{KIND_LABELS[node.kind]} {parent ? `/ ${parent.name}` : ''}</div>
      </div>

      <div className="p-4 space-y-5">
        <Field label="NAME">
          <TextInput value={node.name} onChange={value => onPatch({ name: value })} />
        </Field>
        <Field label="STABLE ID">
          <TextInput value={node.id} onChange={value => onPatch({ id: slugify(value) })} />
        </Field>

        {(node.kind === 'text' || node.kind === 'button') && (
          <Field label="TEXT">
            <TextInput value={node.text ?? ''} onChange={value => onPatch({ text: value })} />
          </Field>
        )}
        {node.kind === 'button' && (
          <Field label="ACTION">
            <TextInput value={node.action ?? ''} onChange={value => onPatch({ action: slugify(value) })} />
          </Field>
        )}
        {node.kind === 'input' && (
          <Field label="PLACEHOLDER">
            <TextInput value={node.placeholder ?? ''} onChange={value => onPatch({ placeholder: value })} />
          </Field>
        )}

        {supportsChildren && (
          <div className="grid grid-cols-2 gap-2">
            {PALETTE_NODE_KINDS.map(kind => (
              <button
                key={kind}
                onClick={() => onAddChild(kind)}
                className="border border-game-border px-2 py-1.5 text-[11px] tracking-widest text-game-textDim hover:text-game-text hover:border-game-primary"
              >
                + {KIND_LABELS[kind]}
              </button>
            ))}
          </div>
        )}

        <StyleInspector node={node} onStyle={onStyle} />

        <div className="grid grid-cols-2 gap-2 pt-2">
          <button
            disabled={isRoot}
            onClick={onDuplicate}
            className="border border-game-border px-2 py-2 text-[11px] tracking-widest text-game-textDim hover:text-game-text hover:border-game-primary disabled:opacity-35"
          >
            DUPLICATE
          </button>
          <button
            disabled={isRoot}
            onClick={onDelete}
            className="border border-game-danger px-2 py-2 text-[11px] tracking-widest text-game-danger hover:bg-game-danger/20 disabled:opacity-35"
          >
            DELETE
          </button>
        </div>
      </div>
    </aside>
  );
}

function StyleInspector({ node, onStyle }: { node: UiNode; onStyle: (style: Partial<UiStyle>) => void }) {
  const style = node.style;
  const supportsHeight = node.kind !== 'button';
  const supportsFont = node.kind === 'text' || node.kind === 'input';
  const supportsTextPalette = node.kind === 'button' || node.kind === 'text';
  const supportsBoxPalette = canHaveChildren(node.kind) || node.kind === 'text';
  const supportsRadius = canHaveChildren(node.kind) || node.kind === 'text';
  return (
    <div className="space-y-4">
      <div className="text-xs tracking-widest text-game-primary">LAYOUT</div>
      <div className="grid grid-cols-2 gap-3">
        <SizeField label="WIDTH" size={style.width} onChange={width => onStyle({ width })} />
        {supportsHeight && (
          <SizeField label="HEIGHT" size={style.height} onChange={height => onStyle({ height })} />
        )}
      </div>

      {canHaveChildren(node.kind) && (
        <>
          <div className="grid grid-cols-2 gap-3">
            <Field label="DIRECTION">
              <Select value={style.direction ?? 'column'} options={AXES} onChange={value => onStyle({ direction: value as UiAxis })} />
            </Field>
            <Field label="GAP">
              <NumberInput value={style.gap ?? 0} min={0} max={64} onChange={gap => onStyle({ gap })} />
            </Field>
          </div>
          <div className="grid grid-cols-2 gap-3">
            <Field label="ALIGN">
              <Select value={style.align ?? 'start'} options={ALIGNS} onChange={value => onStyle({ align: value as UiAlign })} />
            </Field>
            <Field label="JUSTIFY">
              <Select value={style.justify ?? 'start'} options={JUSTIFIES} onChange={value => onStyle({ justify: value as UiJustify })} />
            </Field>
          </div>
        </>
      )}

      <div className="grid grid-cols-2 gap-3">
        <Field label="PADDING">
          <NumberInput value={style.padding ?? 0} min={0} max={96} onChange={padding => onStyle({ padding })} />
        </Field>
        {supportsRadius && (
          <Field label="RADIUS">
            <NumberInput value={style.radius ?? 0} min={0} max={8} onChange={radius => onStyle({ radius })} />
          </Field>
        )}
      </div>

      <div className="text-xs tracking-widest text-game-primary">STYLE</div>
      <div className="grid grid-cols-2 gap-3">
        {supportsFont && (
          <Field label="FONT">
            <Select value={style.font ?? 'ui'} options={FONTS} onChange={value => onStyle({ font: value as UiFont })} />
          </Field>
        )}
      </div>
      <div className="grid grid-cols-3 gap-3">
        {supportsBoxPalette && (
          <>
            <Field label="BG IDX">
              <NumberInput value={style.backgroundPalette ?? -1} min={-1} max={255} onChange={backgroundPalette => onStyle({ backgroundPalette })} />
            </Field>
            <Field label="BORDER IDX">
              <NumberInput value={style.borderPalette ?? -1} min={-1} max={255} onChange={borderPalette => onStyle({ borderPalette })} />
            </Field>
          </>
        )}
        {supportsTextPalette && (
          <Field label="TEXT IDX">
            <NumberInput value={style.textPalette ?? 0} min={0} max={255} onChange={textPalette => onStyle({ textPalette })} />
          </Field>
        )}
      </div>
    </div>
  );
}

function SizeField({ label, size, onChange }: { label: string; size: UiSize; onChange: (size: UiSize) => void }) {
  return (
    <Field label={label}>
      <div className="grid grid-cols-[1fr_72px] gap-2">
        <Select
          value={size.mode}
          options={SIZE_MODES}
          onChange={value => onChange({ mode: value as UiSizeMode, value: size.value ?? 120 })}
        />
        <NumberInput
          value={size.value ?? 0}
          disabled={size.mode !== 'fixed'}
          min={0}
          max={1600}
          onChange={value => onChange({ ...size, value })}
        />
      </div>
    </Field>
  );
}
