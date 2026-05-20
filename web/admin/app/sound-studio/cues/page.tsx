'use client';
import { useCallback, useEffect, useRef, useState } from 'react';
import ReactFlow, {
  addEdge,
  Background,
  Connection,
  Controls,
  Edge,
  Handle,
  MiniMap,
  Node,
  NodeProps,
  Position,
  ReactFlowProvider,
  useEdgesState,
  useNodesState,
  useReactFlow,
} from 'reactflow';
import 'reactflow/dist/style.css';
import { useAuth } from '../../../lib/auth';
import Sidebar from '../../../components/Sidebar';
import { apiFetch } from '../../../lib/api';
import * as store from '../../../lib/sound-cue-store';
import type { CueEdge, CueListEntry, CueNode, CueNodeData, SoundCue } from '../../../lib/sound-cue-store';

const getToken = () => typeof window !== 'undefined' ? localStorage.getItem('zs_token') : '';

// ─── Palette ─────────────────────────────────────────────────────────────────
const NODE_COLORS: Record<string, string> = {
  WavePlayer: '#2a5c8a',
  Random:     '#5c3d8a',
  Sequence:   '#3d6e2f',
  Mixer:      '#7a4a1e',
  Delay:      '#1e6e6e',
  Volume:     '#6e4a1e',
  Pitch:      '#6e1e6e',
  Output:     '#3a3a3a',
};

const NODE_LABELS: Record<string, string> = {
  WavePlayer: 'Wave Player',
  Random:     'Random',
  Sequence:   'Sequence',
  Mixer:      'Mixer',
  Delay:      'Delay',
  Volume:     'Volume',
  Pitch:      'Pitch',
  Output:     'Output',
};

// ─── Shared node shell ───────────────────────────────────────────────────────
function NodeShell({ type, children, hasInput = true, hasOutput = true }:
  { type: string; children?: React.ReactNode; hasInput?: boolean; hasOutput?: boolean }) {
  return (
    <div style={{
      background: '#1a1a1f', border: `2px solid ${NODE_COLORS[type] ?? '#444'}`,
      borderRadius: 6, minWidth: 180, fontSize: 12, color: '#ccc',
    }}>
      <div style={{
        background: NODE_COLORS[type] ?? '#444', padding: '4px 8px',
        borderRadius: '4px 4px 0 0', fontWeight: 600, color: '#fff', fontSize: 13,
      }}>
        {NODE_LABELS[type] ?? type}
      </div>
      {hasInput && <Handle type="target" position={Position.Left} id="in" style={{ background: '#aaa' }} />}
      {hasOutput && <Handle type="source" position={Position.Right} id="out" style={{ background: '#aaa' }} />}
      {children && <div style={{ padding: '6px 10px' }}>{children}</div>}
    </div>
  );
}

// ─── WavePlayer node ─────────────────────────────────────────────────────────
function WavePlayerNode({ data, id }: NodeProps) {
  return (
    <NodeShell type="WavePlayer" hasInput={false}>
      <div style={{ color: data.file ? '#8cf' : '#888', fontStyle: data.file ? 'normal' : 'italic' }}>
        {data.file || 'no file selected'}
      </div>
      <div style={{ color: '#666', fontSize: 11 }}>weight: {data.weight ?? 1}</div>
    </NodeShell>
  );
}

// ─── Multi-input node (Random / Sequence / Mixer) ────────────────────────────
function MultiInputNode({ data, id, type }: NodeProps & { type: string }) {
  const { setNodes } = useReactFlow();
  const inputCount = data._inputCount ?? 2;
  const handles = Array.from({ length: inputCount }, (_, i) => i);
  const baseTop = 36;
  const spacing = type === 'Mixer' ? 52 : 24;

  function addInput() {
    setNodes(nds => nds.map(n => {
      if (n.id !== id) return n;
      const next = (n.data._inputCount ?? 2) + 1;
      const vols = n.type === 'Mixer' ? [...(n.data.mixerVolumes ?? []), 1.0] : n.data.mixerVolumes;
      return { ...n, data: { ...n.data, _inputCount: next, ...(vols !== undefined ? { mixerVolumes: vols } : {}) } };
    }));
  }

  function setMixerVol(portIdx: number, val: number) {
    setNodes(nds => nds.map(n => {
      if (n.id !== id) return n;
      const vols = [...(n.data.mixerVolumes ?? handles.map(() => 1.0))];
      vols[portIdx] = val;
      return { ...n, data: { ...n.data, mixerVolumes: vols } };
    }));
  }

  return (
    <div style={{
      background: '#1a1a1f', border: `2px solid ${NODE_COLORS[type]}`,
      borderRadius: 6, minWidth: 180, fontSize: 12, color: '#ccc', position: 'relative',
      paddingBottom: 4,
    }}>
      <div style={{
        background: NODE_COLORS[type], padding: '4px 8px',
        borderRadius: '4px 4px 0 0', fontWeight: 600, color: '#fff', fontSize: 13,
      }}>
        {NODE_LABELS[type]}
        {type === 'Sequence' && <span style={{ fontWeight: 400, fontSize: 11, marginLeft: 6 }}>
          {data.shuffle ? '(shuffle)' : '(ordered)'}
        </span>}
      </div>
      {handles.map(i => (
        <Handle
          key={i} type="target" position={Position.Left} id={`in-${i}`}
          style={{ top: baseTop + i * spacing, background: '#aaa' }}
        />
      ))}
      <Handle type="source" position={Position.Right} id="out" style={{ background: '#aaa' }} />
      <div style={{ padding: '4px 10px', minHeight: handles.length * spacing }}>
        {type === 'Mixer' && handles.map(i => {
          const vol = (data.mixerVolumes ?? [])[i] ?? 1.0;
          return (
            <div key={i} style={{ marginBottom: 4 }}>
              <div style={{ fontSize: 10, color: '#888', marginBottom: 2 }}>
                in-{i} &nbsp;<span style={{ color: '#aaa' }}>×{vol.toFixed(2)}</span>
              </div>
              <input
                type="range" min={0} max={2} step={0.01} value={vol}
                className="nodrag"
                onChange={e => setMixerVol(i, parseFloat(e.target.value))}
                style={{ width: 120, accentColor: NODE_COLORS['Mixer'] }}
              />
            </div>
          );
        })}
      </div>
      <div
        onClick={addInput}
        style={{ padding: '2px 10px 4px', color: '#5a8', cursor: 'pointer', fontSize: 11 }}
      >
        ⊕ Add input
      </div>
    </div>
  );
}

function RandomNode(props: NodeProps)   { return <MultiInputNode {...props} type="Random" />; }
function SequenceNode(props: NodeProps) { return <MultiInputNode {...props} type="Sequence" />; }
function MixerNode(props: NodeProps)    { return <MultiInputNode {...props} type="Mixer" />; }

// ─── Single-param nodes ───────────────────────────────────────────────────────
function DelayNode({ data }: NodeProps) {
  return (
    <NodeShell type="Delay">
      <div style={{ fontSize: 11 }}>
        min: {data.minSec ?? 0}s &nbsp; max: {data.maxSec ?? 0}s
      </div>
    </NodeShell>
  );
}

function VolumeNode({ data }: NodeProps) {
  return (
    <NodeShell type="Volume">
      <div style={{ fontSize: 11 }}>scalar: ×{(data.scalar ?? 1).toFixed(2)}</div>
    </NodeShell>
  );
}

function PitchNode({ data }: NodeProps) {
  return (
    <NodeShell type="Pitch">
      <div style={{ fontSize: 11 }}>
        {(data.semitones ?? 0) >= 0 ? '+' : ''}{data.semitones ?? 0} st
      </div>
    </NodeShell>
  );
}

// ─── Output node ─────────────────────────────────────────────────────────────
function OutputNode() {
  return (
    <div style={{
      background: '#222', border: '2px solid #555', borderRadius: 8,
      width: 80, height: 80, display: 'flex', alignItems: 'center',
      justifyContent: 'center', fontSize: 28,
    }}>
      <Handle type="target" position={Position.Left} id="in" style={{ background: '#aaa' }} />
      🔊
    </div>
  );
}

const NODE_TYPES = {
  WavePlayer: WavePlayerNode,
  Random:     RandomNode,
  Sequence:   SequenceNode,
  Mixer:      MixerNode,
  Delay:      DelayNode,
  Volume:     VolumeNode,
  Pitch:      PitchNode,
  Output:     OutputNode,
};

// ─── Cue → ReactFlow conversion ───────────────────────────────────────────────
function cueToFlow(cue: SoundCue): { nodes: Node[]; edges: Edge[] } {
  // Count how many inputs each multi-input node has
  const inputCounts: Record<string, number> = {};
  for (const e of cue.edges) {
    const m = e.targetHandle?.match(/^in-(\d+)$/);
    if (m) inputCounts[e.target] = Math.max(inputCounts[e.target] ?? 0, parseInt(m[1]) + 1);
  }

  const nodes: Node[] = cue.nodes.map(n => ({
    id: n.id,
    type: n.type,
    position: n.position,
    data: { ...n.data, _inputCount: inputCounts[n.id] ?? 2 },
  }));

  const edges: Edge[] = cue.edges.map(e => ({
    id: e.id,
    source: e.source,
    sourceHandle: e.sourceHandle,
    target: e.target,
    targetHandle: e.targetHandle,
    style: { stroke: '#666' },
  }));

  return { nodes, edges };
}

// ─── Flow → Cue conversion ────────────────────────────────────────────────────
function flowToCue(id: string, nodes: Node[], edges: Edge[]): SoundCue {
  return {
    id,
    nodes: nodes.map(n => ({
      id: n.id,
      type: n.type as CueNode['type'],
      position: n.position,
      data: Object.fromEntries(
        Object.entries(n.data as CueNodeData).filter(([k]) => k !== '_inputCount')
      ) as CueNodeData,
    })),
    edges: edges.map(e => ({
      id: e.id,
      source: e.source,
      sourceHandle: e.sourceHandle ?? 'out',
      target: e.target,
      targetHandle: e.targetHandle ?? 'in',
    })),
  };
}

// ─── Evaluate cue client-side (mirrors C++ logic) ────────────────────────────
function evalCue(cue: SoundCue): string | null {
  const nodeMap = Object.fromEntries(cue.nodes.map(n => [n.id, n]));
  const edgesTo: Record<string, CueEdge[]> = {};
  for (const e of cue.edges) {
    (edgesTo[e.target] ??= []).push(e);
  }

  const seqCounters: Record<string, number> = {};

  function evalNode(id: string): string | null {
    const n = nodeMap[id];
    if (!n) return null;
    const inputs = (edgesTo[id] ?? [])
      .sort((a, b) => {
        const ai = parseInt(a.targetHandle?.replace('in-', '') ?? '0');
        const bi = parseInt(b.targetHandle?.replace('in-', '') ?? '0');
        return ai - bi;
      })
      .map(e => e.source);

    switch (n.type) {
      case 'WavePlayer': return n.data.file ?? null;
      case 'Random': {
        if (!inputs.length) return null;
        const weights = inputs.map(inp => {
          const w = nodeMap[inp]?.data?.weight;
          return typeof w === 'number' && w > 0 ? w : 1;
        });
        const total = weights.reduce((a, b) => a + b, 0);
        let draw = Math.random() * total;
        for (let i = 0; i < inputs.length; i++) {
          draw -= weights[i];
          if (draw < 0) return evalNode(inputs[i]);
        }
        return evalNode(inputs[inputs.length - 1]);
      }
      case 'Sequence': {
        if (!inputs.length) return null;
        const idx = (seqCounters[id] ?? 0) % inputs.length;
        seqCounters[id] = idx + 1;
        return evalNode(inputs[idx]);
      }
      case 'Mixer':
      case 'Delay':
      case 'Volume':
      case 'Pitch':
        return inputs.length ? evalNode(inputs[0]) : null;
      case 'Output':
        return inputs.length ? evalNode(inputs[0]) : null;
    }
    return null;
  }

  const output = cue.nodes.find(n => n.type === 'Output');
  if (!output) return null;
  return evalNode(output.id);
}

// ─── Canvas component ─────────────────────────────────────────────────────────
function CueCanvas({ cue, onChange }: { cue: SoundCue; onChange: (c: SoundCue) => void }) {
  const { nodes: initNodes, edges: initEdges } = cueToFlow(cue);
  const [nodes, setNodes, onNodesChange] = useNodesState(initNodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initEdges);

  // Sync up to parent on change
  useEffect(() => {
    onChange(flowToCue(cue.id, nodes, edges));
  }, [nodes, edges]); // eslint-disable-line react-hooks/exhaustive-deps

  // Validate connections: no cycles, enforce single-input arity on Output/Delay/Volume/Pitch
  const isValidConnection = useCallback((conn: Connection) => {
    const SINGLE_INPUT = new Set(['Output', 'Delay', 'Volume', 'Pitch']);
    const targetNode = nodes.find(n => n.id === conn.target);
    if (!targetNode) return false;

    // Single-input nodes: reject if any edge already targets this handle
    if (SINGLE_INPUT.has(targetNode.type ?? '')) {
      const handle = conn.targetHandle ?? 'in';
      if (edges.some(e => e.target === conn.target && (e.targetHandle ?? 'in') === handle)) return false;
    }

    // No self-connections
    if (conn.source === conn.target) return false;

    // Cycle check: would adding source→target create a path from target back to source?
    const adjOut: Record<string, string[]> = {};
    for (const e of edges) (adjOut[e.source] ??= []).push(e.target);

    function reaches(from: string, goal: string, seen = new Set<string>()): boolean {
      if (from === goal) return true;
      if (seen.has(from)) return false;
      seen.add(from);
      return (adjOut[from] ?? []).some(n => reaches(n, goal, seen));
    }

    return !reaches(conn.target ?? '', conn.source ?? '');
  }, [nodes, edges]);

  const onConnect = useCallback((conn: Connection) => {
    setEdges(eds => addEdge({ ...conn, style: { stroke: '#666' } }, eds));
    // When wiring a new input to Mixer, ensure mixerVolumes stays in sync
    const portIdx = parseInt(conn.targetHandle?.replace('in-', '') ?? '0');
    setNodes(nds => nds.map(n => {
      if (n.id !== conn.target || n.type !== 'Mixer') return n;
      const vols = [...(n.data.mixerVolumes ?? [])];
      while (vols.length <= portIdx) vols.push(1.0);
      return { ...n, data: { ...n.data, mixerVolumes: vols } };
    }));
  }, [setEdges, setNodes]);

  return (
    <div style={{ flex: 1, height: '100%' }}>
      <ReactFlow
        nodes={nodes} edges={edges}
        onNodesChange={onNodesChange} onEdgesChange={onEdgesChange}
        onConnect={onConnect}
        isValidConnection={isValidConnection}
        nodeTypes={NODE_TYPES}
        fitView
        style={{ background: '#0e0e12' }}
      >
        <Background color="#333" gap={20} />
        <Controls style={{ background: '#1a1a1f', border: '1px solid #333' }} />
        <MiniMap style={{ background: '#111' }} nodeColor={n => NODE_COLORS[n.type ?? ''] ?? '#444'} />
      </ReactFlow>
    </div>
  );
}

// ─── Add-node helper ──────────────────────────────────────────────────────────
let _nodeCounter = 100;
function makeNode(type: CueNode['type']): CueNode {
  const defaults: CueNodeData = (() => {
    switch (type) {
      case 'WavePlayer': return { file: '', weight: 1 };
      case 'Sequence':   return { shuffle: false };
      case 'Mixer':      return { mixerVolumes: [1, 1] };
      case 'Delay':      return { minSec: 0, maxSec: 0.1 };
      case 'Volume':     return { scalar: 1 };
      case 'Pitch':      return { semitones: 0 };
      default:           return {};
    }
  })();
  return {
    id: `n${_nodeCounter++}`,
    type,
    position: { x: 200 + Math.random() * 100, y: 100 + Math.random() * 200 },
    data: defaults,
  };
}

function makeOutputNode(): CueNode {
  return { id: 'out', type: 'Output', position: { x: 700, y: 200 }, data: {} };
}

// ─── Main page ────────────────────────────────────────────────────────────────
export default function SoundCuePage() {
  useAuth();
  const [cueList, setCueList] = useState<CueListEntry[]>(store.getList());
  const [openCue, setOpenCue] = useState<SoundCue | null>(store.getOpenCue());
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);
  const [status, setStatus] = useState('');
  const [newCueName, setNewCueName] = useState('');
  const [creating, setCreating] = useState(false);
  const audioCtxRef = useRef<AudioContext | null>(null);

  // Load cue list
  useEffect(() => {
    if (store.getList().length === 0) {
      apiFetch('/sound-cues')
        .then(list => { store.setList(list as CueListEntry[]); setCueList(list as CueListEntry[]); })
        .catch(() => {});
    }
  }, []);

  async function openCueById(id: string) {
    const cue = await apiFetch(`/sound-cues/${id}`) as SoundCue;
    store.setOpenCue(cue);
    setOpenCue(cue);
    setDirty(false);
  }

  function handleCueChange(updated: SoundCue) {
    setOpenCue(updated);
    store.markDirty();
    setDirty(true);
  }

  async function save() {
    if (!openCue) return;
    setSaving(true);
    try {
      await apiFetch(`/sound-cues/${openCue.id}`, {
        method: 'PUT',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify(openCue),
      });
      store.setOpenCue(openCue);
      setDirty(false);
      setStatus('Saved');
      setTimeout(() => setStatus(''), 2000);
      // Refresh list
      const list = await apiFetch('/sound-cues') as CueListEntry[];
      store.setList(list); setCueList(list);
    } catch (e: any) {
      setStatus(`Error: ${e.message}`);
    } finally {
      setSaving(false);
    }
  }

  async function createCue() {
    if (!newCueName.trim()) return;
    const id = newCueName.trim().toLowerCase().replace(/[^a-z0-9_-]/g, '_');
    const blank: SoundCue = {
      id,
      nodes: [makeOutputNode()],
      edges: [],
    };
    await apiFetch(`/sound-cues/${id}`, {
      method: 'PUT',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(blank),
    });
    const list = await apiFetch('/sound-cues') as CueListEntry[];
    store.setList(list); setCueList(list);
    setNewCueName(''); setCreating(false);
    openCueById(id);
  }

  async function deleteCue(id: string) {
    if (!confirm(`Delete cue "${id}"?`)) return;
    await apiFetch(`/sound-cues/${id}`, {
      method: 'DELETE',
    });
    if (openCue?.id === id) { setOpenCue(null); store.setOpenCue(null); }
    const list = await apiFetch('/sound-cues') as CueListEntry[];
    store.setList(list); setCueList(list);
  }

  function addNode(type: CueNode['type']) {
    if (!openCue) return;
    const node = makeNode(type);
    handleCueChange({ ...openCue, nodes: [...openCue.nodes, node] });
  }

  async function playCue() {
    if (!openCue) return;
    const file = evalCue(openCue);
    if (!file) { setStatus('No sound resolved'); setTimeout(() => setStatus(''), 2000); return; }
    try {
      const token = getToken();
      const resp = await fetch(`${process.env.NEXT_PUBLIC_API_URL || 'http://localhost:4000'}/api/sounds/${encodeURIComponent(file)}/play`, {
        headers: token ? { Authorization: `Bearer ${token}` } : {},
      });
      const buf = await resp.arrayBuffer();
      if (!audioCtxRef.current) audioCtxRef.current = new AudioContext();
      const decoded = await audioCtxRef.current.decodeAudioData(buf);
      const src = audioCtxRef.current.createBufferSource();
      src.buffer = decoded;
      src.connect(audioCtxRef.current.destination);
      src.start();
      setStatus(`▶ ${file}`);
      setTimeout(() => setStatus(''), 3000);
    } catch (e: any) {
      setStatus(`Play failed: ${e.message}`);
    }
  }

  const NODE_TYPE_LIST: CueNode['type'][] = [
    'WavePlayer', 'Random', 'Sequence', 'Mixer', 'Delay', 'Volume', 'Pitch',
  ];

  return (
    <div style={{ display: 'flex', height: '100vh', background: '#0e0e12', color: '#ccc', fontFamily: 'sans-serif' }}>
      <Sidebar />

      {/* ── Cue list panel ── */}
      <div style={{ width: 220, borderRight: '1px solid #2a2a30', display: 'flex', flexDirection: 'column', overflowY: 'auto' }}>
        <div style={{ padding: '12px 10px 6px', fontWeight: 700, fontSize: 13, color: '#fff', borderBottom: '1px solid #2a2a30' }}>
          Sound Cues
        </div>
        {cueList.map(c => (
          <div key={c.id}
            onClick={() => openCueById(c.id)}
            style={{
              padding: '7px 10px', cursor: 'pointer', fontSize: 12,
              background: openCue?.id === c.id ? '#1e2a38' : 'transparent',
              borderLeft: openCue?.id === c.id ? '3px solid #4a8fd4' : '3px solid transparent',
              display: 'flex', justifyContent: 'space-between', alignItems: 'center',
            }}
          >
            <span style={{ color: openCue?.id === c.id ? '#8cf' : '#bbb' }}>{c.id}</span>
            <button
              onClick={e => { e.stopPropagation(); deleteCue(c.id); }}
              style={{ background: 'none', border: 'none', color: '#666', cursor: 'pointer', fontSize: 14 }}
            >✕</button>
          </div>
        ))}

        {creating ? (
          <div style={{ padding: '8px 10px', display: 'flex', gap: 4 }}>
            <input
              autoFocus value={newCueName} onChange={e => setNewCueName(e.target.value)}
              onKeyDown={e => { if (e.key === 'Enter') createCue(); if (e.key === 'Escape') setCreating(false); }}
              placeholder="cue_id"
              style={{ flex: 1, background: '#1a1a1f', border: '1px solid #444', color: '#fff', borderRadius: 4, padding: '3px 6px', fontSize: 12 }}
            />
            <button onClick={createCue} style={{ background: '#2a5c8a', border: 'none', color: '#fff', borderRadius: 4, padding: '3px 8px', cursor: 'pointer', fontSize: 12 }}>✓</button>
          </div>
        ) : (
          <button
            onClick={() => setCreating(true)}
            style={{ margin: 8, background: '#1e2a1e', border: '1px solid #3a5a3a', color: '#8c8', borderRadius: 4, padding: '5px 8px', cursor: 'pointer', fontSize: 12 }}
          >
            + New Cue
          </button>
        )}
      </div>

      {/* ── Main canvas area ── */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
        {/* Toolbar */}
        <div style={{ height: 42, borderBottom: '1px solid #2a2a30', display: 'flex', alignItems: 'center', gap: 8, padding: '0 12px', background: '#13131a' }}>
          {openCue && <>
            <span style={{ fontWeight: 600, color: '#8cf', fontSize: 13 }}>{openCue.id}</span>
            {dirty && <span style={{ color: '#fa4', fontSize: 11 }}>●</span>}
            <button onClick={save} disabled={!dirty || saving}
              style={{ background: '#1e4a1e', border: '1px solid #3a7a3a', color: '#8c8', borderRadius: 4, padding: '3px 12px', cursor: 'pointer', fontSize: 12, opacity: dirty ? 1 : 0.4 }}>
              {saving ? 'Saving…' : 'Save'}
            </button>
            <button onClick={playCue}
              style={{ background: '#1e2a4a', border: '1px solid #3a5a8a', color: '#8af', borderRadius: 4, padding: '3px 12px', cursor: 'pointer', fontSize: 12 }}>
              ▶ Play
            </button>
            <div style={{ position: 'relative' }}>
              <select
                defaultValue=""
                onChange={e => { if (e.target.value) { addNode(e.target.value as CueNode['type']); e.target.value = ''; } }}
                style={{ background: '#1a1a2a', border: '1px solid #444', color: '#ccc', borderRadius: 4, padding: '3px 8px', cursor: 'pointer', fontSize: 12 }}
              >
                <option value="" disabled>+ Add Node</option>
                {NODE_TYPE_LIST.map(t => <option key={t} value={t}>{NODE_LABELS[t]}</option>)}
              </select>
            </div>
            {status && <span style={{ fontSize: 11, color: status.startsWith('Error') ? '#f88' : '#8c8', marginLeft: 8 }}>{status}</span>}
          </>}
          {!openCue && <span style={{ color: '#555', fontSize: 12 }}>Select or create a cue to edit</span>}
        </div>

        {/* Canvas */}
        {openCue ? (
          <ReactFlowProvider>
            <CueCanvas key={openCue.id} cue={openCue} onChange={handleCueChange} />
          </ReactFlowProvider>
        ) : (
          <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#333', fontSize: 14 }}>
            No cue open
          </div>
        )}
      </div>
    </div>
  );
}
