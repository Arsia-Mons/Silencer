import { PRESETS } from "../ui-editor-constants";
import {
  type UiDocument,
  type UiDocumentReference,
  normalizeUiSurface,
} from "../../../lib/ui-layout";
import { ToolbarButton } from "./EditorControls";

interface EditorTopBarProps {
  document: UiDocument;
  documents: UiDocumentReference[];
  status: string;
  zoom: number;
  onZoom: (zoom: number) => void;
  onDocumentChange: (document: UiDocument) => void;
  onLoadDocument: (surface: string) => void;
  onPreset: (preset: (typeof PRESETS)[number]) => void;
  onImport: () => void;
  onSave: () => void;
  onDownloadJson: () => void;
  onReset: () => void;
}

export function EditorTopBar(props: EditorTopBarProps) {
  const {
    document,
    documents,
    status,
    zoom,
    onZoom,
    onDocumentChange,
    onLoadDocument,
    onPreset,
    onImport,
    onSave,
    onDownloadJson,
    onReset,
  } = props;
  const currentSurface = normalizeUiSurface(document.surface);
  const documentOptions = documents.some((candidate) => candidate.surface === currentSurface)
    ? documents
    : [
        {
          surface: currentSurface,
          filename: `${currentSurface}.silencer-ui.json`,
          title: `${currentSurface} (unsaved)`,
          updatedAt: "",
          revision: "",
        },
        ...documents,
      ];
  return (
    <header className="px-5 py-3 bg-game-bgCard border-b border-game-border flex items-center gap-4">
      <div className="min-w-0">
        <h1 className="text-xl font-bold tracking-widest text-game-primary">UI EDITOR</h1>
        <div className="text-[11px] tracking-wider text-game-textDim">{status}</div>
      </div>
      <label className="ml-4 text-[11px] tracking-widest text-game-textDim">
        DOCUMENT
        <select
          className="block mt-1 w-52 bg-game-bg border border-game-border px-2 py-1 text-game-text focus:outline-none focus:border-game-primary"
          value={currentSurface}
          onChange={(event) => onLoadDocument(event.target.value)}
        >
          {documentOptions.map((candidate) => (
            <option key={candidate.surface} value={candidate.surface}>
              {candidate.title}
            </option>
          ))}
        </select>
      </label>
      <label className="ml-4 text-[11px] tracking-widest text-game-textDim">
        SURFACE
        <input
          className="block mt-1 w-44 bg-game-bg border border-game-border px-2 py-1 text-game-text focus:outline-none focus:border-game-primary"
          value={document.surface}
          onChange={(event) =>
            onDocumentChange({ ...document, surface: normalizeUiSurface(event.target.value) })
          }
        />
      </label>
      <label className="text-[11px] tracking-widest text-game-textDim">
        VIEWPORT
        <select
          className="block mt-1 bg-game-bg border border-game-border px-2 py-1 text-game-text focus:outline-none focus:border-game-primary"
          value={`${document.viewport.width}x${document.viewport.height}`}
          onChange={(event) => {
            const preset = PRESETS.find(
              (candidate) => `${candidate.width}x${candidate.height}` === event.target.value,
            );
            if (preset) onPreset(preset);
          }}
        >
          {PRESETS.map((preset) => (
            <option key={preset.name} value={`${preset.width}x${preset.height}`}>
              {preset.name}
            </option>
          ))}
        </select>
      </label>
      <label className="text-[11px] tracking-widest text-game-textDim">
        ZOOM
        <input
          className="block mt-2 w-32 accent-game-primary"
          type="range"
          min="0.35"
          max="1.25"
          step="0.01"
          value={zoom}
          onChange={(event) => onZoom(Number(event.target.value))}
        />
      </label>
      <div className="ml-auto flex items-center gap-2">
        <ToolbarButton onClick={onSave}>SAVE</ToolbarButton>
        <ToolbarButton onClick={onImport}>IMPORT</ToolbarButton>
        <ToolbarButton onClick={onDownloadJson}>JSON</ToolbarButton>
        <ToolbarButton onClick={onReset}>RESET</ToolbarButton>
      </div>
    </header>
  );
}
