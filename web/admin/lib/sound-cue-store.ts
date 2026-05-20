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
}

export interface CueNode {
  id: string;
  type: 'WavePlayer' | 'Random' | 'Sequence' | 'Mixer' | 'Delay' | 'Volume' | 'Pitch' | 'Output';
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
