import { KIND_LABELS } from "../ui-editor-constants";
import { slugify } from "../ui-editor-utils";
import {
  PALETTE_NODE_KINDS,
  UI_ALIGNS,
  UI_ATTACH_POINTS,
  UI_ATTACH_TO_VALUES,
  UI_AXES,
  UI_BUTTON_SIZES,
  UI_BUTTON_VARIANTS,
  UI_FONTS,
  UI_FORBIDDEN_NODE_DECORATORS_BY_KIND,
  UI_IMAGE_MODES,
  UI_JUSTIFIES,
  UI_NUMERIC_LIMITS,
  UI_SIZE_MODES,
  UI_SIZE_RULES_BY_KIND,
  UI_STYLE_FIELDS_BY_KIND,
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
  type UiSurfaceTokenManifest,
} from "../../../lib/ui-layout";
import { Field, NumberInput, Select, TextInput } from "./EditorControls";

const BUTTON_VARIANTS: UiButtonVariant[] = [...UI_BUTTON_VARIANTS];
const BUTTON_SIZES: UiButtonSize[] = [...UI_BUTTON_SIZES];
const IMAGE_MODES: UiImageMode[] = [...UI_IMAGE_MODES];
const ATTACH_TO: UiAttachTo[] = [...UI_ATTACH_TO_VALUES];
const ATTACH_POINTS: UiAttachPoint[] = [...UI_ATTACH_POINTS];
const AXES: UiAxis[] = [...UI_AXES];
const ALIGNS: UiAlign[] = [...UI_ALIGNS];
const JUSTIFIES: UiJustify[] = [...UI_JUSTIFIES];
const FONTS: UiFont[] = [...UI_FONTS];
const SIZE_MODES: UiSizeMode[] = [...UI_SIZE_MODES];

interface InspectorProps {
  node: UiNode;
  parent: UiNode | null;
  isRoot: boolean;
  tokenManifest: UiSurfaceTokenManifest | null;
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
  tokenManifest,
  onPatch,
  onStyle,
  onDelete,
  onDuplicate,
  onAddChild,
}: InspectorProps) {
  const supportsChildren = canHaveChildren(node.kind);
  const forbiddenDecorators = new Set<string>(UI_FORBIDDEN_NODE_DECORATORS_BY_KIND[node.kind]);
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
            <TokenField
              label="ACTION"
              value={node.action ?? ""}
              options={tokenManifest?.actions ?? []}
              onChange={(value) => onPatch({ action: value.trim() })}
            />
            <TokenField
              label="TEXT BINDING"
              value={node.textBinding ?? ""}
              options={tokenManifest?.textBindings ?? []}
              onChange={(value) => onPatch({ textBinding: value.trim() || undefined })}
            />
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
          <TokenField
            label="COMPONENT"
            value={node.component ?? ""}
            options={tokenManifest?.components ?? []}
            onChange={(value) => onPatch({ component: value.trim() })}
          />
        )}
        {node.kind === "text" && (
          <TokenField
            label="TEXT BINDING"
            value={node.textBinding ?? ""}
            options={tokenManifest?.textBindings ?? []}
            onChange={(value) => onPatch({ textBinding: value.trim() || undefined })}
          />
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
        {!forbiddenDecorators.has("image") && <ImageInspector node={node} onPatch={onPatch} />}
        {!forbiddenDecorators.has("floating") && (
          <FloatingInspector node={node} onPatch={onPatch} />
        )}

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

function TokenField({
  label,
  value,
  options,
  onChange,
}: {
  label: string;
  value: string;
  options: readonly string[];
  onChange: (value: string) => void;
}) {
  const choices = uniqueOptions(["", value, ...options]);
  if (choices.length <= 1) {
    return (
      <Field label={label}>
        <input
          disabled
          value="NO SURFACE TOKENS"
          className="w-full bg-game-bg border border-game-border px-2 py-1.5 text-game-textDim disabled:opacity-50"
        />
      </Field>
    );
  }
  return (
    <Field label={label}>
      <Select value={value} options={choices} onChange={onChange} />
    </Field>
  );
}

function uniqueOptions(values: readonly string[]): string[] {
  return Array.from(new Set(values));
}

function StyleInspector({
  node,
  onStyle,
}: {
  node: UiNode;
  onStyle: (style: Partial<UiStyle>) => void;
}) {
  const style = node.style;
  const fields = new Set<string>(UI_STYLE_FIELDS_BY_KIND[node.kind]);
  const widthRule = sizeRuleFor(node.kind, "width");
  const heightRule = sizeRuleFor(node.kind, "height");
  return (
    <div className="space-y-4">
      <div className="text-xs tracking-widest text-game-primary">LAYOUT</div>
      <div className="grid grid-cols-2 gap-3">
        <SizeField
          label="WIDTH"
          size={style.width}
          modes={widthRule?.modes ?? SIZE_MODES}
          supportsBounds={widthRule?.allowBounds ?? true}
          onChange={(width) => onStyle({ width })}
        />
        {fields.has("height") && (
          <SizeField
            label="HEIGHT"
            size={style.height}
            modes={heightRule?.modes ?? SIZE_MODES}
            supportsBounds={heightRule?.allowBounds ?? true}
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
                min={UI_NUMERIC_LIMITS.gapMin}
                max={UI_NUMERIC_LIMITS.gapMax}
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
        {fields.has("padding") && (
          <Field label="PADDING">
            <NumberInput
              value={style.padding ?? 0}
              min={UI_NUMERIC_LIMITS.paddingMin}
              max={UI_NUMERIC_LIMITS.paddingMax}
              onChange={(padding) => onStyle({ padding })}
            />
          </Field>
        )}
        {fields.has("radius") && (
          <Field label="RADIUS">
            <NumberInput
              value={style.radius ?? 0}
              min={UI_NUMERIC_LIMITS.radiusMin}
              max={UI_NUMERIC_LIMITS.radiusMax}
              onChange={(radius) => onStyle({ radius })}
            />
          </Field>
        )}
      </div>

      <div className="text-xs tracking-widest text-game-primary">STYLE</div>
      <div className="grid grid-cols-2 gap-3">
        {fields.has("font") && (
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
        {fields.has("backgroundPalette") && (
          <>
            <Field label="BG IDX">
              <NumberInput
                value={style.backgroundPalette ?? -1}
                min={UI_NUMERIC_LIMITS.paletteMin}
                max={UI_NUMERIC_LIMITS.paletteMax}
                onChange={(backgroundPalette) => onStyle({ backgroundPalette })}
              />
            </Field>
          </>
        )}
        {fields.has("borderPalette") && (
          <>
            <Field label="BORDER IDX">
              <NumberInput
                value={style.borderPalette ?? -1}
                min={UI_NUMERIC_LIMITS.paletteMin}
                max={UI_NUMERIC_LIMITS.paletteMax}
                onChange={(borderPalette) => onStyle({ borderPalette })}
              />
            </Field>
          </>
        )}
        {fields.has("textPalette") && (
          <Field label="TEXT IDX">
            <NumberInput
              value={style.textPalette ?? 0}
              min={UI_NUMERIC_LIMITS.paletteTextMin}
              max={UI_NUMERIC_LIMITS.paletteMax}
              onChange={(textPalette) => onStyle({ textPalette })}
            />
          </Field>
        )}
      </div>
    </div>
  );
}

function sizeRuleFor(kind: UiNodeKind, axis: "width" | "height") {
  const rules = UI_SIZE_RULES_BY_KIND[kind as keyof typeof UI_SIZE_RULES_BY_KIND] as
    | Partial<Record<"width" | "height", { modes: readonly UiSizeMode[]; allowBounds: boolean }>>
    | undefined;
  return rules?.[axis];
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
              min={UI_NUMERIC_LIMITS.imageBankMin}
              max={UI_NUMERIC_LIMITS.imageBankMax}
              onChange={(bank) => onPatch({ image: { ...image, bank } })}
            />
          </Field>
          <Field label="INDEX">
            <NumberInput
              value={image.index}
              min={UI_NUMERIC_LIMITS.imageIndexMin}
              max={UI_NUMERIC_LIMITS.imageIndexMax}
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
                min={UI_NUMERIC_LIMITS.floatingZIndexMin}
                max={UI_NUMERIC_LIMITS.floatingZIndexMax}
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
                min={UI_NUMERIC_LIMITS.floatingOffsetMin}
                max={UI_NUMERIC_LIMITS.floatingOffsetMax}
                onChange={(offsetX) => onPatch({ floating: { ...floating, offsetX } })}
              />
            </Field>
            <Field label="Y">
              <NumberInput
                value={floating.offsetY ?? 0}
                min={UI_NUMERIC_LIMITS.floatingOffsetMin}
                max={UI_NUMERIC_LIMITS.floatingOffsetMax}
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
  modes?: readonly UiSizeMode[];
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
            min={UI_NUMERIC_LIMITS.sizeMin}
            max={UI_NUMERIC_LIMITS.sizeMax}
            onChange={(value) => onChange({ ...size, value })}
          />
        </div>
        {supportsBounds && size.mode !== "fixed" && (
          <div className="grid grid-cols-2 gap-2">
            <label className="block text-[10px] tracking-widest text-game-textDim">
              <span className="mb-1 block">MIN</span>
              <NumberInput
                value={size.min ?? 0}
                min={UI_NUMERIC_LIMITS.sizeMin}
                max={UI_NUMERIC_LIMITS.sizeMax}
                onChange={(min) => onChange({ ...size, min: min > 0 ? min : undefined })}
              />
            </label>
            <label className="block text-[10px] tracking-widest text-game-textDim">
              <span className="mb-1 block">MAX</span>
              <NumberInput
                value={size.max ?? 0}
                min={UI_NUMERIC_LIMITS.sizeMin}
                max={UI_NUMERIC_LIMITS.sizeMax}
                onChange={(max) => onChange({ ...size, max: max > 0 ? max : undefined })}
              />
            </label>
          </div>
        )}
      </div>
    </Field>
  );
}
