import { ALIGNS, AXES, FONTS, JUSTIFIES, KIND_LABELS, SIZE_MODES } from "../ui-editor-constants";
import { slugify } from "../ui-editor-utils";
import {
  PALETTE_NODE_KINDS,
  UI_ATTACH_POINTS,
  UI_ATTACH_TO_VALUES,
  UI_BUTTON_SIZES,
  UI_BUTTON_VARIANTS,
  UI_IMAGE_MODES,
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

const BUTTON_VARIANTS: UiButtonVariant[] = [...UI_BUTTON_VARIANTS];
const BUTTON_SIZES: UiButtonSize[] = [...UI_BUTTON_SIZES];
const IMAGE_MODES: UiImageMode[] = [...UI_IMAGE_MODES];
const ATTACH_TO: UiAttachTo[] = [...UI_ATTACH_TO_VALUES];
const ATTACH_POINTS: UiAttachPoint[] = [...UI_ATTACH_POINTS];

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
  const supportsNodeDecorators = node.kind !== "button";
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
              <TextInput
                value={node.action ?? ""}
                onChange={(value) => onPatch({ action: value.trim() })}
              />
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
        {supportsNodeDecorators && <ImageInspector node={node} onPatch={onPatch} />}
        {supportsNodeDecorators && <FloatingInspector node={node} onPatch={onPatch} />}

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
  const sizeModes: UiSizeMode[] =
    node.kind === "button" ? SIZE_MODES.filter((mode) => mode !== "grow") : SIZE_MODES;
  const supportsSizeBounds = node.kind !== "button";
  const supportsPadding = node.kind !== "spacer";
  const supportsFont = node.kind === "text";
  const supportsTextPalette = node.kind === "button" || node.kind === "text";
  const supportsBoxPalette = canHaveChildren(node.kind) || node.kind === "text";
  const supportsRadius = canHaveChildren(node.kind) || node.kind === "text";
  return (
    <div className="space-y-4">
      <div className="text-xs tracking-widest text-game-primary">LAYOUT</div>
      <div className="grid grid-cols-2 gap-3">
        <SizeField
          label="WIDTH"
          size={style.width}
          modes={sizeModes}
          supportsBounds={supportsSizeBounds}
          onChange={(width) => onStyle({ width })}
        />
        {supportsHeight && (
          <SizeField
            label="HEIGHT"
            size={style.height}
            modes={SIZE_MODES}
            supportsBounds={supportsSizeBounds}
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
  const image = node.image ?? { bank: 0, index: 0, mode: "normal" as UiImageMode };
  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between">
        <div className="text-xs tracking-widest text-game-primary">IMAGE</div>
        <button
          onClick={() => onPatch({ image: node.image ? undefined : image })}
          className="border border-game-border px-2 py-1 text-[10px] tracking-widest text-game-textDim hover:text-game-text hover:border-game-primary"
        >
          {node.image ? "REMOVE" : "ADD"}
        </button>
      </div>
      {!node.image ? null : (
        <div className="grid grid-cols-3 gap-3">
          <Field label="BANK">
            <NumberInput
              value={image.bank}
              min={0}
              max={255}
              onChange={(bank) => onPatch({ image: { ...image, bank } })}
            />
          </Field>
          <Field label="INDEX">
            <NumberInput
              value={image.index}
              min={0}
              max={65535}
              onChange={(index) => onPatch({ image: { ...image, index } })}
            />
          </Field>
          <Field label="MODE">
            <Select
              value={image.mode ?? "normal"}
              options={IMAGE_MODES}
              onChange={(mode) => onPatch({ image: { ...image, mode: mode as UiImageMode } })}
            />
          </Field>
        </div>
      )}
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
  const floating = node.floating ?? {
    attachTo: "parent" as UiAttachTo,
    elementAttach: "left-top" as UiAttachPoint,
    parentAttach: "left-top" as UiAttachPoint,
  };
  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between">
        <div className="text-xs tracking-widest text-game-primary">FLOATING</div>
        <button
          onClick={() => onPatch({ floating: node.floating ? undefined : floating })}
          className="border border-game-border px-2 py-1 text-[10px] tracking-widest text-game-textDim hover:text-game-text hover:border-game-primary"
        >
          {node.floating ? "REMOVE" : "ADD"}
        </button>
      </div>
      {!node.floating ? null : (
        <>
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
                  onPatch({
                    floating: { ...floating, elementAttach: elementAttach as UiAttachPoint },
                  })
                }
              />
            </Field>
            <Field label="PARENT">
              <Select
                value={floating.parentAttach}
                options={ATTACH_POINTS}
                onChange={(parentAttach) =>
                  onPatch({
                    floating: { ...floating, parentAttach: parentAttach as UiAttachPoint },
                  })
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
          <label className="flex items-center gap-2 text-[11px] tracking-widest text-game-textDim">
            <input
              type="checkbox"
              checked={floating.pointerPassthrough ?? false}
              onChange={(event) =>
                onPatch({
                  floating: {
                    ...floating,
                    pointerPassthrough: event.target.checked ? true : undefined,
                  },
                })
              }
              className="accent-game-primary"
            />
            POINTER PASSTHROUGH
          </label>
        </>
      )}
    </div>
  );
}

function SizeField({
  label,
  size,
  modes = SIZE_MODES,
  supportsBounds = true,
  onChange,
}: {
  label: string;
  size: UiSize;
  modes?: UiSizeMode[];
  supportsBounds?: boolean;
  onChange: (size: UiSize) => void;
}) {
  const changeMode = (mode: UiSizeMode) => {
    if (mode === "fixed") {
      onChange({ mode, value: size.value ?? size.min ?? 120 });
      return;
    }
    onChange({
      mode,
      ...(size.min !== undefined ? { min: size.min } : {}),
      ...(size.max !== undefined ? { max: size.max } : {}),
    });
  };

  return (
    <Field label={label}>
      <div className="space-y-2">
        <div className="grid grid-cols-[1fr_72px] gap-2">
          <Select
            value={size.mode}
            options={modes}
            onChange={(value) => changeMode(value as UiSizeMode)}
          />
          <NumberInput
            value={size.value ?? 0}
            disabled={size.mode !== "fixed"}
            min={0}
            max={1600}
            onChange={(value) => onChange({ ...size, value })}
          />
        </div>
        {supportsBounds && size.mode !== "fixed" && (
          <div className="grid grid-cols-2 gap-2">
            <label className="block text-[10px] tracking-widest text-game-textDim">
              <span className="mb-1 block">MIN</span>
              <NumberInput
                value={size.min ?? 0}
                min={0}
                max={4096}
                onChange={(min) => onChange({ ...size, min: min > 0 ? min : undefined })}
              />
            </label>
            <label className="block text-[10px] tracking-widest text-game-textDim">
              <span className="mb-1 block">MAX</span>
              <NumberInput
                value={size.max ?? 0}
                min={0}
                max={4096}
                onChange={(max) => onChange({ ...size, max: max > 0 ? max : undefined })}
              />
            </label>
          </div>
        )}
      </div>
    </Field>
  );
}
