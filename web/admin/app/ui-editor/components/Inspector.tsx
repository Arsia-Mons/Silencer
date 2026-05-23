import { ALIGNS, AXES, FONTS, JUSTIFIES, KIND_LABELS, SIZE_MODES } from "../ui-editor-constants";
import { slugify } from "../ui-editor-utils";
import {
  PALETTE_NODE_KINDS,
  canHaveChildren,
  type UiAlign,
  type UiAttachPoint,
  type UiAttachTo,
  type UiAxis,
  type UiButtonSize,
  type UiButtonVariant,
  type UiFont,
  type UiImageMode,
  type UiJustify,
  type UiNode,
  type UiNodeKind,
  type UiSize,
  type UiSizeMode,
  type UiStyle,
} from "../../../lib/ui-layout";
import { Field, NumberInput, Select, TextInput } from "./EditorControls";

const BUTTON_VARIANTS: UiButtonVariant[] = ["oval", "chrome", "text", "ghost"];
const BUTTON_SIZES: UiButtonSize[] = ["sm", "md", "lg", "compact", "auto"];
const IMAGE_MODES: UiImageMode[] = ["normal", "contain", "stretch"];
const ATTACH_TO: UiAttachTo[] = ["parent", "root"];
const ATTACH_POINTS: UiAttachPoint[] = [
  "left-top",
  "left-center",
  "left-bottom",
  "center-top",
  "center",
  "center-bottom",
  "right-top",
  "right-center",
  "right-bottom",
];

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

export function Inspector({
  node,
  parent,
  isRoot,
  onPatch,
  onStyle,
  onDelete,
  onDuplicate,
  onAddChild,
}: InspectorProps) {
  const supportsChildren = canHaveChildren(node.kind);
  return (
    <aside className="min-h-0 overflow-auto border-l border-game-border bg-game-bgCard/95">
      <div className="p-4 border-b border-game-border">
        <div className="text-xs tracking-widest text-game-primary mb-3">INSPECTOR</div>
        <div className="text-[11px] tracking-widest text-game-textDim">
          {KIND_LABELS[node.kind]} {parent ? `/ ${parent.name}` : ""}
        </div>
      </div>

      <div className="p-4 space-y-5">
        <Field label="NAME">
          <TextInput value={node.name} onChange={(value) => onPatch({ name: value })} />
        </Field>
        <Field label="STABLE ID">
          <TextInput value={node.id} onChange={(value) => onPatch({ id: slugify(value) })} />
        </Field>

        {(node.kind === "text" || node.kind === "button") && (
          <Field label="TEXT">
            <TextInput value={node.text ?? ""} onChange={(value) => onPatch({ text: value })} />
          </Field>
        )}
        {node.kind === "button" && (
          <div className="space-y-3">
            <Field label="ACTION">
              <TextInput value={node.action ?? ""} onChange={(value) => onPatch({ action: value.trim() })} />
            </Field>
            <div className="grid grid-cols-2 gap-3">
              <Field label="VARIANT">
                <Select
                  value={node.buttonVariant ?? "chrome"}
                  options={BUTTON_VARIANTS}
                  onChange={(value) => onPatch({ buttonVariant: value as UiButtonVariant })}
                />
              </Field>
              <Field label="SIZE">
                <Select
                  value={node.buttonSize ?? "auto"}
                  options={BUTTON_SIZES}
                  onChange={(value) => onPatch({ buttonSize: value as UiButtonSize })}
                />
              </Field>
            </div>
          </div>
        )}
        {node.kind === "input" && (
          <Field label="PLACEHOLDER">
            <TextInput
              value={node.placeholder ?? ""}
              onChange={(value) => onPatch({ placeholder: value })}
            />
          </Field>
        )}
        {node.kind === "component" && (
          <Field label="COMPONENT">
            <TextInput
              value={node.component ?? ""}
              onChange={(value) => onPatch({ component: value.trim() })}
            />
          </Field>
        )}
        {node.kind === "text" && (
          <Field label="TEXT BINDING">
            <TextInput
              value={node.textBinding ?? ""}
              onChange={(value) => onPatch({ textBinding: value.trim() || undefined })}
            />
          </Field>
        )}

        {supportsChildren && (
          <div className="grid grid-cols-2 gap-2">
            {PALETTE_NODE_KINDS.map((kind) => (
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
        <ImageInspector node={node} onPatch={onPatch} />
        <FloatingInspector node={node} onPatch={onPatch} />

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

function StyleInspector({
  node,
  onStyle,
}: {
  node: UiNode;
  onStyle: (style: Partial<UiStyle>) => void;
}) {
  const style = node.style;
  const supportsHeight = node.kind !== "button";
  const supportsPadding = node.kind !== "spacer";
  const supportsFont = node.kind === "text" || node.kind === "input";
  const supportsTextPalette = node.kind === "button" || node.kind === "text";
  const supportsBoxPalette = canHaveChildren(node.kind) || node.kind === "text";
  const supportsRadius = canHaveChildren(node.kind) || node.kind === "text";
  return (
    <div className="space-y-4">
      <div className="text-xs tracking-widest text-game-primary">LAYOUT</div>
      <div className="grid grid-cols-2 gap-3">
        <SizeField label="WIDTH" size={style.width} onChange={(width) => onStyle({ width })} />
        {supportsHeight && (
          <SizeField
            label="HEIGHT"
            size={style.height}
            onChange={(height) => onStyle({ height })}
          />
        )}
      </div>

      {canHaveChildren(node.kind) && (
        <>
          <div className="grid grid-cols-2 gap-3">
            <Field label="DIRECTION">
              <Select
                value={style.direction ?? "column"}
                options={AXES}
                onChange={(value) => onStyle({ direction: value as UiAxis })}
              />
            </Field>
            <Field label="GAP">
              <NumberInput
                value={style.gap ?? 0}
                min={0}
                max={64}
                onChange={(gap) => onStyle({ gap })}
              />
            </Field>
          </div>
          <div className="grid grid-cols-2 gap-3">
            <Field label="ALIGN">
              <Select
                value={style.align ?? "start"}
                options={ALIGNS}
                onChange={(value) => onStyle({ align: value as UiAlign })}
              />
            </Field>
            <Field label="JUSTIFY">
              <Select
                value={style.justify ?? "start"}
                options={JUSTIFIES}
                onChange={(value) => onStyle({ justify: value as UiJustify })}
              />
            </Field>
          </div>
        </>
      )}

      <div className="grid grid-cols-2 gap-3">
        {supportsPadding && (
          <Field label="PADDING">
            <NumberInput
              value={style.padding ?? 0}
              min={0}
              max={96}
              onChange={(padding) => onStyle({ padding })}
            />
          </Field>
        )}
        {supportsRadius && (
          <Field label="RADIUS">
            <NumberInput
              value={style.radius ?? 0}
              min={0}
              max={8}
              onChange={(radius) => onStyle({ radius })}
            />
          </Field>
        )}
      </div>

      <div className="text-xs tracking-widest text-game-primary">STYLE</div>
      <div className="grid grid-cols-2 gap-3">
        {supportsFont && (
          <Field label="FONT">
            <Select
              value={style.font ?? "ui"}
              options={FONTS}
              onChange={(value) => onStyle({ font: value as UiFont })}
            />
          </Field>
        )}
      </div>
      <div className="grid grid-cols-3 gap-3">
        {supportsBoxPalette && (
          <>
            <Field label="BG IDX">
              <NumberInput
                value={style.backgroundPalette ?? -1}
                min={-1}
                max={255}
                onChange={(backgroundPalette) => onStyle({ backgroundPalette })}
              />
            </Field>
            <Field label="BORDER IDX">
              <NumberInput
                value={style.borderPalette ?? -1}
                min={-1}
                max={255}
                onChange={(borderPalette) => onStyle({ borderPalette })}
              />
            </Field>
          </>
        )}
        {supportsTextPalette && (
          <Field label="TEXT IDX">
            <NumberInput
              value={style.textPalette ?? 0}
              min={0}
              max={255}
              onChange={(textPalette) => onStyle({ textPalette })}
            />
          </Field>
        )}
      </div>
    </div>
  );
}

function ImageInspector({
  node,
  onPatch,
}: {
  node: UiNode;
  onPatch: (patch: Partial<UiNode>) => void;
}) {
  if (!node.image) return null;
  return (
    <div className="space-y-3">
      <div className="text-xs tracking-widest text-game-primary">IMAGE</div>
      <div className="grid grid-cols-3 gap-3">
        <Field label="BANK">
          <NumberInput
            value={node.image.bank}
            min={0}
            max={255}
            onChange={(bank) => onPatch({ image: { ...node.image!, bank } })}
          />
        </Field>
        <Field label="INDEX">
          <NumberInput
            value={node.image.index}
            min={0}
            max={65535}
            onChange={(index) => onPatch({ image: { ...node.image!, index } })}
          />
        </Field>
        <Field label="MODE">
          <Select
            value={node.image.mode ?? "normal"}
            options={IMAGE_MODES}
            onChange={(mode) => onPatch({ image: { ...node.image!, mode: mode as UiImageMode } })}
          />
        </Field>
      </div>
    </div>
  );
}

function FloatingInspector({
  node,
  onPatch,
}: {
  node: UiNode;
  onPatch: (patch: Partial<UiNode>) => void;
}) {
  if (!node.floating) return null;
  const floating = node.floating;
  return (
    <div className="space-y-3">
      <div className="text-xs tracking-widest text-game-primary">FLOATING</div>
      <div className="grid grid-cols-2 gap-3">
        <Field label="ATTACH TO">
          <Select
            value={floating.attachTo}
            options={ATTACH_TO}
            onChange={(attachTo) =>
              onPatch({ floating: { ...floating, attachTo: attachTo as UiAttachTo } })
            }
          />
        </Field>
        <Field label="Z">
          <NumberInput
            value={floating.zIndex ?? 0}
            min={-32768}
            max={32767}
            onChange={(zIndex) => onPatch({ floating: { ...floating, zIndex } })}
          />
        </Field>
      </div>
      <div className="grid grid-cols-2 gap-3">
        <Field label="ELEMENT">
          <Select
            value={floating.elementAttach}
            options={ATTACH_POINTS}
            onChange={(elementAttach) =>
              onPatch({ floating: { ...floating, elementAttach: elementAttach as UiAttachPoint } })
            }
          />
        </Field>
        <Field label="PARENT">
          <Select
            value={floating.parentAttach}
            options={ATTACH_POINTS}
            onChange={(parentAttach) =>
              onPatch({ floating: { ...floating, parentAttach: parentAttach as UiAttachPoint } })
            }
          />
        </Field>
      </div>
      <div className="grid grid-cols-2 gap-3">
        <Field label="X">
          <NumberInput
            value={floating.offsetX ?? 0}
            min={-4096}
            max={4096}
            onChange={(offsetX) => onPatch({ floating: { ...floating, offsetX } })}
          />
        </Field>
        <Field label="Y">
          <NumberInput
            value={floating.offsetY ?? 0}
            min={-4096}
            max={4096}
            onChange={(offsetY) => onPatch({ floating: { ...floating, offsetY } })}
          />
        </Field>
      </div>
    </div>
  );
}

function SizeField({
  label,
  size,
  onChange,
}: {
  label: string;
  size: UiSize;
  onChange: (size: UiSize) => void;
}) {
  return (
    <Field label={label}>
      <div className="grid grid-cols-[1fr_72px] gap-2">
        <Select
          value={size.mode}
          options={SIZE_MODES}
          onChange={(value) => onChange({ ...size, mode: value as UiSizeMode, value: size.value ?? 120 })}
        />
        <NumberInput
          value={size.value ?? 0}
          disabled={size.mode !== "fixed"}
          min={0}
          max={1600}
          onChange={(value) => onChange({ ...size, value })}
        />
      </div>
    </Field>
  );
}
