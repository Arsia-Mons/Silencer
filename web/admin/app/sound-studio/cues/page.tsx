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
import { decodeAdpcmWav } from '../../sound-studio/adpcm';

const getToken = () => typeof window !== 'undefined' ? localStorage.getItem('zs_token') : '';

// ─── Silencer green palette ───────────────────────────────────────────────────
const C = {
  bg:      '#050a05',
  card:    '#0a120a',
  border:  '#1a2e1a',
  primary: '#00a328',
  dark:    '#005b1c',
  light:   '#0fa835',
  muted:   '#4a7a4a',
  text:    '#d1fad7',
  dim:     '#7ab87a',
};

const NODE_COLORS: Record<string, string> = {
  WavePlayer: '#005b1c',
  Random:     '#006b35',
  Sequence:   '#1a5c3a',
  Mixer:      '#2a5c1a',
  Delay:      '#005b40',
  Volume:     '#1a5c10',
  Pitch:      '#3a5c05',
  Output:     '#00a328',
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

// ─── Shared node shell ────────────────────────────────────────────────────────
function NodeShell({ type, children, hasInput = true, hasOutput = true }:
  { type: string; children?: React.ReactNode; hasInput?: boolean; hasOutput?: boolean }) {
  return (
    <div style={{
      background: C.card, border: `2px solid ${NODE_COLORS[type] ?? C.muted}`,
      borderRadius: 6, minWidth: 180, fontSize: 12, color: C.text,
    }}>
      <div style={{
        background: NODE_COLORS[type] ?? C.muted, padding: '4px 8px',
        borderRadius: '4px 4px 0 0', fontWeight: 600, color: '#fff', fontSize: 13,
      }}>
        {NODE_LABELS[type] ?? type}
      </div>
      {hasInput  && <Handle type="target" position={Position.Left}  id="in"  style={{ background: C.light, border: '1px solid #fff' }} />}
      {hasOutput && <Handle type="source" position={Position.Right} id="out" style={{ background: C.primary, border: '1px solid #fff' }} />}
      {children && <div style={{ padding: '6px 10px' }}>{children}</div>}
    </div>
  );
}

// ─── WavePlayer node ──────────────────────────────────────────────────────────
function WavePlayerNode({ data }: NodeProps) {
  return (
    <NodeShell type="WavePlayer" hasInput={false}>
      <div style={{ color: data.file ? C.light : C.muted, fontStyle: data.file ? 'normal' : 'italic' }}>
        {data.file || 'no file selected'}
      </div>
      <div style={{ color: C.dim, fontSize: 11 }}>weight: {data.weight ?? 1}</div>
    </NodeShell>
  );
}

// ─── Multi-input node (Random / Sequence / Mixer) ─────────────────────────────
function MultiInputNode({ data, id, type }: NodeProps & { type: string }) {
  const { setNodes, setEdges } = useReactFlow();
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

  function removeInput() {
    const cur = inputCount;
    if (cur <= 1) return;
    const removedHandle = `in-${cur - 1}`;
    setNodes(nds => nds.map(n => {
      if (n.id !== id) return n;
      const next = cur - 1;
      const vols = n.type === 'Mixer'
        ? (n.data.mixerVolumes ?? []).slice(0, next)
        : n.data.mixerVolumes;
      return { ...n, data: { ...n.data, _inputCount: next, ...(vols !== undefined ? { mixerVolumes: vols } : {}) } };
    }));
    setEdges(eds => eds.filter(e => !(e.target === id && e.targetHandle === removedHandle)));
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
      background: C.card, border: `2px solid ${NODE_COLORS[type]}`,
      borderRadius: 6, minWidth: 180, fontSize: 12, color: C.text, position: 'relative',
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
          style={{ top: baseTop + i * spacing, background: C.light, border: '1px solid #fff' }}
        />
      ))}
      <Handle type="source" position={Position.Right} id="out" style={{ background: C.primary, border: '1px solid #fff' }} />
      <div style={{ padding: '4px 10px', minHeight: handles.length * spacing }}>
        {type === 'Mixer' && handles.map(i => {
          const vol = (data.mixerVolumes ?? [])[i] ?? 1.0;
          return (
            <div key={i} style={{ marginBottom: 4 }}>
              <div style={{ fontSize: 10, color: C.dim, marginBottom: 2 }}>
                in-{i} &nbsp;<span style={{ color: C.text }}>×{vol.toFixed(2)}</span>
              </div>
              <input
                type="range" min={0} max={2} step={0.01} value={vol}
                className="nodrag"
                onChange={e => setMixerVol(i, parseFloat(e.target.value))}
                style={{ width: 120, accentColor: C.primary }}
              />
            </div>
          );
        })}
      </div>
      <div style={{ display: 'flex', gap: 6, padding: '2px 10px 4px' }}>
        <span onClick={addInput}    style={{ color: C.light,   cursor: 'pointer', fontSize: 11 }}>⊕ Add</span>
        <span onClick={removeInput} style={{ color: inputCount <= 1 ? C.muted : '#ef4444', cursor: inputCount <= 1 ? 'default' : 'pointer', fontSize: 11 }}>⊖ Remove</span>
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
      <div style={{ fontSize: 11 }}>min: {data.minSec ?? 0}s &nbsp; max: {data.maxSec ?? 0}s</div>
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

// ─── Output node ──────────────────────────────────────────────────────────────
function OutputNode() {
  return (
    <div style={{
      background: C.card, border: `2px solid ${C.primary}`, borderRadius: 8,
      width: 80, height: 80, display: 'flex', alignItems: 'center',
      justifyContent: 'center', fontSize: 28,
    }}>
      <Handle type="target" position={Position.Left} id="in" style={{ background: C.light, border: '1px solid #fff' }} />
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

// ─── Cue ↔ ReactFlow conversion ───────────────────────────────────────────────
function cueToFlow(cue: SoundCue): { nodes: Node[]; edges: Edge[] } {
  const inputCounts: Record<string, number> = {};
  for (const e of cue.edges) {
    const m = e.targetHandle?.match(/^in-(\d+)$/);
    if (m) inputCounts[e.target] = Math.max(inputCounts[e.target] ?? 0, parseInt(m[1]) + 1);
  }
  const nodes: Node[] = cue.nodes.map(n => ({
    id: n.id, type: n.type, position: n.position,
    data: { ...n.data, _inputCount: inputCounts[n.id] ?? 2 },
  }));
  const edges: Edge[] = cue.edges.map(e => ({
    id: e.id, source: e.source, sourceHandle: e.sourceHandle,
    target: e.target, targetHandle: e.targetHandle,
    style: { stroke: C.muted },
  }));
  return { nodes, edges };
}

function flowToCue(id: string, nodes: Node[], edges: Edge[]): SoundCue {
  return {
    id,
    nodes: nodes.map(n => ({
      id: n.id, type: n.type as CueNode['type'], position: n.position,
      data: Object.fromEntries(
        Object.entries(n.data as CueNodeData).filter(([k]) => k !== '_inputCount')
      ) as CueNodeData,
    })),
    edges: edges.map(e => ({
      id: e.id, source: e.source, sourceHandle: e.sourceHandle ?? 'out',
      target: e.target, targetHandle: e.targetHandle ?? 'in',
    })),
  };
}

// ─── Evaluate cue client-side (mirrors C++ logic) ────────────────────────────
// Returns the resolved file AND the ordered list of node IDs on the winning path.
function evalCue(cue: SoundCue): { file: string | null; path: string[] } {
  const nodeMap = Object.fromEntries(cue.nodes.map(n => [n.id, n]));
  const edgesTo: Record<string, CueEdge[]> = {};
  for (const e of cue.edges) (edgesTo[e.target] ??= []).push(e);
  const seqCounters: Record<string, number> = {};
  const path: string[] = [];

  function evalNode(id: string): string | null {
    path.push(id);
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
      case 'Output':
        return inputs.length ? evalNode(inputs[0]) : null;
    }
    return null;
  }

  const output = cue.nodes.find(n => n.type === 'Output');
  const file = output ? evalNode(output.id) : null;
  return { file, path };
}

// ─── Node factory ─────────────────────────────────────────────────────────────
let _nodeCounter = 100;
function makeNode(type: CueNode['type'], position?: { x: number; y: number }): Node {
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
    position: position ?? { x: 200 + Math.random() * 120, y: 80 + Math.random() * 200 },
    data: { ...defaults, _inputCount: 2 },
  };
}

const NODE_TYPE_LIST: CueNode['type'][] = [
  'WavePlayer', 'Random', 'Sequence', 'Mixer', 'Delay', 'Volume', 'Pitch',
];

// ─── Canvas + right palette ───────────────────────────────────────────────────
function CueCanvas({ cue, onChange, activePath }: {
  cue: SoundCue;
  onChange: (c: SoundCue) => void;
  activePath: string[];
}) {
  const { nodes: initNodes, edges: initEdges } = cueToFlow(cue);
  const [nodes, setNodes, onNodesChange] = useNodesState(initNodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initEdges);
  const wrapperRef = useRef<HTMLDivElement>(null);
  const { screenToFlowPosition } = useReactFlow();

  useEffect(() => {
    onChange(flowToCue(cue.id, nodes, edges));
  }, [nodes, edges]); // eslint-disable-line react-hooks/exhaustive-deps

  // Highlight the active path when Play is pressed
  useEffect(() => {
    if (!activePath.length) return;
    const pathSet = new Set(activePath);
    setNodes(nds => nds.map(n => ({
      ...n,
      style: pathSet.has(n.id)
        ? { filter: 'drop-shadow(0 0 6px #00a328) drop-shadow(0 0 16px #0fa835)', zIndex: 10 }
        : { opacity: 0.35 },
    })));
    setEdges(eds => eds.map(e => {
      const active = pathSet.has(e.source) && pathSet.has(e.target);
      return { ...e, style: { stroke: active ? '#0fa835' : '#1a2e1a', strokeWidth: active ? 3 : 1, transition: 'stroke 0.2s, stroke-width 0.2s' } };
    }));
    const t = setTimeout(() => {
      setNodes(nds => nds.map(n => ({ ...n, style: {} })));
      setEdges(eds => eds.map(e => ({ ...e, style: { stroke: C.muted } })));
    }, 3000);
    return () => clearTimeout(t);
  }, [activePath]); // eslint-disable-line react-hooks/exhaustive-deps

  const isValidConnection = useCallback((conn: Connection) => {
    const SINGLE_INPUT = new Set(['Output', 'Delay', 'Volume', 'Pitch']);
    const targetNode = nodes.find(n => n.id === conn.target);
    if (!targetNode) return false;
    if (SINGLE_INPUT.has(targetNode.type ?? '')) {
      const handle = conn.targetHandle ?? 'in';
      if (edges.some(e => e.target === conn.target && (e.targetHandle ?? 'in') === handle)) return false;
    }
    if (conn.source === conn.target) return false;
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
    setEdges(eds => addEdge({ ...conn, style: { stroke: C.muted } }, eds));
    const portIdx = parseInt(conn.targetHandle?.replace('in-', '') ?? '0');
    setNodes(nds => nds.map(n => {
      if (n.id !== conn.target || n.type !== 'Mixer') return n;
      const vols = [...(n.data.mixerVolumes ?? [])];
      while (vols.length <= portIdx) vols.push(1.0);
      return { ...n, data: { ...n.data, mixerVolumes: vols } };
    }));
  }, [setEdges, setNodes]);

  function onDrop(e: React.DragEvent) {
    e.preventDefault();
    const type = e.dataTransfer.getData('application/reactflow') as CueNode['type'];
    if (!type || !NODE_TYPES[type]) return;
    const position = screenToFlowPosition({ x: e.clientX, y: e.clientY });
    setNodes(nds => [...nds, makeNode(type, position)]);
  }

  function addNodeAtCenter(type: CueNode['type']) {
    setNodes(nds => [...nds, makeNode(type)]);
  }

  return (
    <div style={{ flex: 1, height: '100%', display: 'flex' }}>
      {/* Canvas */}
      <div ref={wrapperRef} style={{ flex: 1, height: '100%' }}
        onDrop={onDrop} onDragOver={e => e.preventDefault()}>
        <ReactFlow
          nodes={nodes} edges={edges}
          onNodesChange={onNodesChange} onEdgesChange={onEdgesChange}
          onConnect={onConnect}
          isValidConnection={isValidConnection}
          nodeTypes={NODE_TYPES}
          fitView
          style={{ background: C.bg }}
        >
          <Background color={C.border} gap={20} />
          <Controls style={{ background: C.card, border: `1px solid ${C.border}` }} />
          <MiniMap style={{ background: C.card }} nodeColor={n => NODE_COLORS[n.type ?? ''] ?? C.muted} />
        </ReactFlow>
      </div>

      {/* Right palette */}
      <div style={{
        width: 140, borderLeft: `1px solid ${C.border}`, background: C.card,
        display: 'flex', flexDirection: 'column', padding: '8px 6px', gap: 6, overflowY: 'auto',
      }}>
        <div style={{ fontSize: 10, color: C.dim, fontWeight: 700, letterSpacing: 1, paddingBottom: 4, borderBottom: `1px solid ${C.border}` }}>
          NODES
        </div>
        {NODE_TYPE_LIST.map(type => (
          <div
            key={type}
            draggable
            onDragStart={e => e.dataTransfer.setData('application/reactflow', type)}
            onClick={() => addNodeAtCenter(type)}
            title={`Drag or click to add ${NODE_LABELS[type]}`}
            style={{
              padding: '7px 8px', background: NODE_COLORS[type], borderRadius: 4,
              color: '#fff', fontSize: 11, fontWeight: 600, cursor: 'grab',
              userSelect: 'none', border: `1px solid rgba(255,255,255,0.1)`,
              boxShadow: '0 1px 4px rgba(0,0,0,0.4)',
              transition: 'opacity 0.1s',
            }}
            onMouseEnter={e => (e.currentTarget.style.opacity = '0.85')}
            onMouseLeave={e => (e.currentTarget.style.opacity = '1')}
          >
            {NODE_LABELS[type]}
          </div>
        ))}
        <div style={{ fontSize: 9, color: C.muted, marginTop: 4, lineHeight: 1.4 }}>
          Drag onto canvas or click to add
        </div>
      </div>
    </div>
  );
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
  const [activePath, setActivePath] = useState<string[]>([]);
  const audioCtxRef = useRef<AudioContext | null>(null);

  useEffect(() => {
    if (store.getList().length === 0) {
      apiFetch('/sound-cues')
        .then(list => { store.setList(list as CueListEntry[]); setCueList(list as CueListEntry[]); })
        .catch(() => {});
    }
  }, []);

  async function openCueById(id: string) {
    const cue = await apiFetch(`/sound-cues/${id}`) as SoundCue;
    store.setOpenCue(cue); setOpenCue(cue); setDirty(false);
  }

  function handleCueChange(updated: SoundCue) {
    setOpenCue(updated); store.markDirty(); setDirty(true);
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
      store.setOpenCue(openCue); setDirty(false);
      setStatus('Saved'); setTimeout(() => setStatus(''), 2000);
      const list = await apiFetch('/sound-cues') as CueListEntry[];
      store.setList(list); setCueList(list);
    } catch (e: any) {
      setStatus(`Error: ${e.message}`);
    } finally { setSaving(false); }
  }

  async function createCue() {
    if (!newCueName.trim()) return;
    const id = newCueName.trim().toLowerCase().replace(/[^a-z0-9_-]/g, '_');
    const blank: SoundCue = { id, nodes: [{ id: 'out', type: 'Output', position: { x: 700, y: 200 }, data: {} }], edges: [] };
    await apiFetch(`/sound-cues/${id}`, {
      method: 'PUT', headers: { 'content-type': 'application/json' }, body: JSON.stringify(blank),
    });
    const list = await apiFetch('/sound-cues') as CueListEntry[];
    store.setList(list); setCueList(list);
    setNewCueName(''); setCreating(false);
    openCueById(id);
  }

  async function deleteCue(id: string) {
    if (!confirm(`Delete cue "${id}"?`)) return;
    await apiFetch(`/sound-cues/${id}`, { method: 'DELETE' });
    if (openCue?.id === id) { setOpenCue(null); store.setOpenCue(null); }
    const list = await apiFetch('/sound-cues') as CueListEntry[];
    store.setList(list); setCueList(list);
  }

  async function playCue() {
    if (!openCue) return;
    const { file, path } = evalCue(openCue);
    if (!file) { setStatus('No sound resolved'); setTimeout(() => setStatus(''), 2000); return; }
    setActivePath(path);
    try {
      const token = getToken();
      const resp = await fetch(`/api/sounds/${encodeURIComponent(file)}/play`, {
        headers: token ? { Authorization: `Bearer ${token}` } : {},
      });
      if (!resp.ok) throw new Error(`${resp.status} ${resp.statusText}`);
      const buf = await resp.arrayBuffer();
      if (!audioCtxRef.current) audioCtxRef.current = new AudioContext();
      if (audioCtxRef.current.state === 'suspended') await audioCtxRef.current.resume();
      const decoded = await decodeAdpcmWav(buf, audioCtxRef.current);
      const src = audioCtxRef.current.createBufferSource();
      src.buffer = decoded; src.connect(audioCtxRef.current.destination); src.start();
      setStatus(`▶ ${file}`); setTimeout(() => setStatus(''), 3000);
    } catch (e: any) { setStatus(`Play failed: ${e.message}`); }
  }

  return (
    <div style={{ display: 'flex', height: '100vh', background: C.bg, color: C.text, fontFamily: 'monospace' }}>
      <Sidebar />

      {/* ── Cue list panel ── */}
      <div style={{ width: 220, borderRight: `1px solid ${C.border}`, display: 'flex', flexDirection: 'column', overflowY: 'auto', background: C.card }}>
        <div style={{ padding: '12px 10px 6px', fontWeight: 700, fontSize: 12, color: C.primary, letterSpacing: 1, borderBottom: `1px solid ${C.border}` }}>
          SOUND CUES
        </div>
        {cueList.map(c => (
          <div key={c.id} onClick={() => openCueById(c.id)} style={{
            padding: '7px 10px', cursor: 'pointer', fontSize: 12,
            background: openCue?.id === c.id ? 'rgba(0,163,40,0.12)' : 'transparent',
            borderLeft: `3px solid ${openCue?.id === c.id ? C.primary : 'transparent'}`,
            display: 'flex', justifyContent: 'space-between', alignItems: 'center',
          }}>
            <span style={{ color: openCue?.id === c.id ? C.light : C.dim }}>{c.id}</span>
            <button onClick={e => { e.stopPropagation(); deleteCue(c.id); }}
              style={{ background: 'none', border: 'none', color: C.muted, cursor: 'pointer', fontSize: 14 }}>✕</button>
          </div>
        ))}

        {creating ? (
          <div style={{ padding: '8px 10px', display: 'flex', gap: 4 }}>
            <input
              autoFocus value={newCueName} onChange={e => setNewCueName(e.target.value)}
              onKeyDown={e => { if (e.key === 'Enter') createCue(); if (e.key === 'Escape') setCreating(false); }}
              placeholder="cue_id"
              style={{ flex: 1, background: C.bg, border: `1px solid ${C.border}`, color: C.text, borderRadius: 4, padding: '3px 6px', fontSize: 12 }}
            />
            <button onClick={createCue}
              style={{ background: C.dark, border: `1px solid ${C.primary}`, color: C.primary, borderRadius: 4, padding: '3px 8px', cursor: 'pointer', fontSize: 12 }}>✓</button>
          </div>
        ) : (
          <button onClick={() => setCreating(true)}
            style={{ margin: 8, background: 'rgba(0,163,40,0.1)', border: `1px solid ${C.dark}`, color: C.primary, borderRadius: 4, padding: '5px 8px', cursor: 'pointer', fontSize: 11, letterSpacing: 0.5 }}>
            + NEW CUE
          </button>
        )}
      </div>

      {/* ── Canvas area ── */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
        {/* Toolbar */}
        <div style={{ height: 42, borderBottom: `1px solid ${C.border}`, display: 'flex', alignItems: 'center', gap: 8, padding: '0 12px', background: C.card }}>
          {openCue && <>
            <span style={{ fontWeight: 700, color: C.primary, fontSize: 13, letterSpacing: 1 }}>{openCue.id}</span>
            {dirty && <span style={{ color: '#f59e0b', fontSize: 11 }}>●</span>}
            <button onClick={save} disabled={!dirty || saving}
              style={{ background: dirty ? 'rgba(0,163,40,0.15)' : 'transparent', border: `1px solid ${dirty ? C.primary : C.border}`, color: dirty ? C.primary : C.muted, borderRadius: 4, padding: '3px 12px', cursor: dirty ? 'pointer' : 'default', fontSize: 11, letterSpacing: 0.5 }}>
              {saving ? 'SAVING…' : 'SAVE'}
            </button>
            <button onClick={playCue}
              style={{ background: 'rgba(15,168,53,0.15)', border: `1px solid ${C.light}`, color: C.light, borderRadius: 4, padding: '3px 12px', cursor: 'pointer', fontSize: 11, letterSpacing: 0.5 }}>
              ▶ PLAY
            </button>
            {status && <span style={{ fontSize: 11, color: status.startsWith('Error') || status.startsWith('Play failed') ? '#ef4444' : C.dim, marginLeft: 8 }}>{status}</span>}
          </>}
          {!openCue && <span style={{ color: C.muted, fontSize: 12 }}>Select or create a cue to edit</span>}
        </div>

        {/* Canvas */}
        {openCue ? (
          <ReactFlowProvider>
            <CueCanvas key={openCue.id} cue={openCue} onChange={handleCueChange} activePath={activePath} />
          </ReactFlowProvider>
        ) : (
          <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', color: C.border, fontSize: 14, letterSpacing: 2 }}>
            NO CUE OPEN
          </div>
        )}
      </div>
    </div>
  );
}
