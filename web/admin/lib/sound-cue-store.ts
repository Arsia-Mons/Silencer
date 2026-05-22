// Module-level singleton — survives client-side navigation.
// Holds the cue list and the currently open cue graph.

export interface CueListEntry {
  id: string;
  nodeCount: number;
  edgeCount: number;
}

export interface CueNodeData {
  // WavePlayer
  file?: string;
  weight?: number;
  // Sequence
  shuffle?: boolean;
  // Mixer: per-port volumes (parallel to input handles)
  mixerVolumes?: number[];
  // Delay
  minSec?: number;
  maxSec?: number;
  // Volume
  scalar?: number;
  // Pitch
  semitones?: number;
  // Modulator
  volumeMin?: number;
  volumeMax?: number;
  pitchMin?: number;
  pitchMax?: number;
  // Looping
  loopCount?: number;
  loopIndefinite?: boolean;
  // Branch
  paramName?: string;
}

export interface CueNode {
  id: string;
  type:
    | 'WavePlayer'
    | 'Random'
    | 'Sequence'
    | 'Mixer'
    | 'Delay'
    | 'Volume'
    | 'Pitch'
    | 'Modulator'
    | 'Concatenator'
    | 'Looping'
    | 'Branch'
    | 'Output';
  position: { x: number; y: number };
  data: CueNodeData;
}

export interface CueEdge {
  id: string;
  source: string;
  sourceHandle: string;
  target: string;
  targetHandle: string;
}

export interface SoundCue {
  id: string;
  nodes: CueNode[];
  edges: CueEdge[];
}

interface SoundCueStoreData {
  cues: CueListEntry[];
  openCue: SoundCue | null;
  dirty: boolean;
}

let _data: SoundCueStoreData = { cues: [], openCue: null, dirty: false };

export function getList(): CueListEntry[] { return _data.cues; }
export function setList(cues: CueListEntry[]): void { _data.cues = cues; }

export function getOpenCue(): SoundCue | null { return _data.openCue; }
export function setOpenCue(cue: SoundCue | null): void { _data.openCue = cue; _data.dirty = false; }

export function isDirty(): boolean { return _data.dirty; }
export function markDirty(): void { _data.dirty = true; }

export function clear(): void { _data = { cues: [], openCue: null, dirty: false }; }

// Pre-loaded cue files from the assets folder picker (sound-studio page).
// When sound studio picks shared/assets/, it also reads gas/sound-cues/*.json
// and stores them here so the cues page can auto-load without a second picker.
let _cueFiles: Record<string, SoundCue> | null = null;
let _cuesFolderName: string | null = null;

export function hasCueFiles(): boolean { return _cueFiles !== null && Object.keys(_cueFiles).length > 0; }
export function getCueFiles(): Record<string, SoundCue> | null { return _cueFiles; }
export function getCuesFolderName(): string | null { return _cuesFolderName; }
export function setCueFiles(files: Record<string, SoundCue>, folderName: string): void {
  _cueFiles = files; _cuesFolderName = folderName;
}
export function clearCueFiles(): void { _cueFiles = null; _cuesFolderName = null; }
