'use client';
/**
 * C6 – State machine editor (ReactFlow)
 * Nodes  = animation sequences
 * Edges  = transitions between states
 * Right panel = inspector for selected edge / node
 */
import { useCallback, useEffect, useMemo, useState } from 'react';
import ReactFlow, {
  ReactFlowProvider,
  Background,
  Controls,
  MiniMap,
  addEdge,
  useNodesState,
  useEdgesState,
  MarkerType,
  type Node,
  type Edge,
  type Connection,
  type NodeProps,
  Handle,
  Position,
  BackgroundVariant,
} from 'reactflow';
import type { ActorDef, StateMachine, StateMachineTransition } from '../../../lib/api';

// ─── Constants ────────────────────────────────────────────────────────────────

const KNOWN_CONDITIONS = [
  'sequence_complete',
  'player_in_range',
  'player_visible',
  'player_lost',
  'hp_low',
  'hp_zero',
  'velocity_nonzero',
  'velocity_zero',
  'grounded',
  'alerted',
  'attack_cooldown',
  'spawn',
];

const GRID_COL = 220;
const GRID_ROW = 140;

// ─── Helpers ──────────────────────────────────────────────────────────────────

function uid() {
  return Math.random().toString(36).slice(2, 9);
}

/** Remove transitions/positions that reference deleted sequences. */
function normalizeSM(sm: StateMachine, seqNames: string[]): StateMachine {
  const valid = new Set(seqNames);
  const transitions = sm.transitions.filter(
    t => valid.has(t.from) && valid.has(t.to),
  );
  const positions: StateMachine['positions'] = {};
  for (const k of seqNames) {
    if (sm.positions[k]) positions[k] = sm.positions[k];
  }
  const initial = sm.initial && valid.has(sm.initial) ? sm.initial : (seqNames[0] ?? null);
  return { initial, transitions, positions };
}

function defaultSM(seqNames: string[]): StateMachine {
  const positions: StateMachine['positions'] = {};
  seqNames.forEach((name, i) => {
    const col = i % 4;
    const row = Math.floor(i / 4);
    positions[name] = { x: col * GRID_COL, y: row * GRID_ROW };
  });
  return { initial: seqNames[0] ?? null, transitions: [], positions };
}

function smToFlow(
  sm: StateMachine,
  seqNames: string[],
  sequences: NonNullable<ActorDef['sequences']>,
): { nodes: Node[]; edges: Edge[] } {
  const nodes: Node[] = seqNames.map(name => ({
    id: name,
    type: 'stateNode',
    position: sm.positions[name] ?? { x: 0, y: 0 },
    data: {
      label: name,
      loop: sequences[name]?.loop ?? false,
      bank: sequences[name]?.frames[0]?.bank,
      frameIdx: sequences[name]?.frames[0]?.index,
      isInitial: sm.initial === name,
      frameCount: sequences[name]?.frames.length ?? 0,
    },
  }));

  const edges: Edge[] = sm.transitions.map(t => ({
    id: t.id,
    source: t.from,
    target: t.to,
    label: t.condition || '—',
    labelStyle: { fill: '#8aff80', fontSize: 10, fontFamily: 'monospace' },
    labelBgStyle: { fill: '#0d1a0d', fillOpacity: 0.85 },
    style: { stroke: '#4a8a4a' },
    markerEnd: { type: MarkerType.ArrowClosed, color: '#4a8a4a' },
    animated: false,
  }));

  return { nodes, edges };
}

// ─── Custom Node ──────────────────────────────────────────────────────────────

function StateNode({ data }: NodeProps) {
  return (
    <div
      className={`border font-mono text-xs select-none ${
        data.isInitial
          ? 'border-game-primary bg-game-primary/20 text-game-primary'
          : 'border-game-border bg-game-bgCard text-game-text'
      }`}
      style={{ minWidth: 140, minHeight: 70 }}
    >
      <Handle type="target" position={Position.Left} style={{ background: '#4a8a4a', borderColor: '#4a8a4a' }} />
      <Handle type="source" position={Position.Right} style={{ background: '#4a8a4a', borderColor: '#4a8a4a' }} />

      <div className="px-2 py-1 border-b border-game-border/50 flex items-center gap-1.5">
        {data.isInitial && <span className="text-game-primary text-[9px]">▶</span>}
        <span className="font-bold tracking-wide truncate">{data.label}</span>
        <span className="ml-auto text-game-textDim text-[9px]">{data.loop ? '↻' : '→'}</span>
      </div>

      <div className="flex items-center gap-2 px-2 py-1.5">
        {data.bank !== undefined && data.frameCount > 0 ? (
          // eslint-disable-next-line @next/next/no-img-element
          <img
            src={`/api/sprites/${data.bank}/${data.frameIdx ?? 0}`}
            alt=""
            style={{ imageRendering: 'pixelated', width: 32, height: 32, objectFit: 'contain' }}
          />
        ) : (
          <div className="w-8 h-8 bg-black/40 flex items-center justify-center text-game-textDim text-[9px]">—</div>
        )}
        <span className="text-game-textDim text-[9px]">{data.frameCount}f</span>
      </div>
    </div>
  );
}

const NODE_TYPES = { stateNode: StateNode };

// ─── Inner (needs ReactFlowProvider) ─────────────────────────────────────────

interface Props {
  def: ActorDef;
  onChange: (sm: StateMachine) => void;
}

function StateMachineInner({ def, onChange }: Props) {
  const seqNames = useMemo(() => Object.keys(def.sequences ?? {}), [def.sequences]);

  const sm = useMemo<StateMachine>(() => {
    const raw = def.stateMachine;
    if (raw && raw.transitions) return normalizeSM(raw as StateMachine, seqNames);
    return defaultSM(seqNames);
  }, [def.stateMachine, seqNames]);

  const { nodes: initNodes, edges: initEdges } = useMemo(
    () => smToFlow(sm, seqNames, def.sequences ?? {}),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [], // intentionally only on mount
  );

  const [nodes, setNodes, onNodesChange] = useNodesState(initNodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initEdges);

  // Keep nodes in sync when sequences change
  useEffect(() => {
    const currentSM = buildCurrentSM();
    const { nodes: n, edges: e } = smToFlow(
      normalizeSM(currentSM, seqNames),
      seqNames,
      def.sequences ?? {},
    );
    setNodes(n);
    setEdges(e);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [seqNames.join(',')]);

  // Inspector state
  const [selectedEdgeId, setSelectedEdgeId]     = useState<string | null>(null);
  const [addFrom, setAddFrom]                   = useState(seqNames[0] ?? '');
  const [addTo, setAddTo]                       = useState(seqNames[1] ?? '');
  const [addCondition, setAddCondition]         = useState('sequence_complete');
  const [conditionEdit, setConditionEdit]       = useState('');
  const [initialEdit, setInitialEdit]           = useState(sm.initial ?? '');

  const selectedEdge = edges.find(e => e.id === selectedEdgeId) ?? null;

  function buildCurrentSM(): StateMachine {
    const positions: StateMachine['positions'] = {};
    for (const n of nodes) positions[n.id] = n.position;
    const transitions: StateMachineTransition[] = edges.map(e => ({
      id: e.id,
      from: e.source,
      to: e.target,
      condition: (e.label as string) === '—' ? '' : String(e.label ?? ''),
    }));
    const initial = nodes.find(n => n.data.isInitial)?.id ?? null;
    return { initial, transitions, positions };
  }

  function emitChange(overrideNodes = nodes, overrideEdges = edges) {
    const positions: StateMachine['positions'] = {};
    for (const n of overrideNodes) positions[n.id] = n.position;
    const transitions: StateMachineTransition[] = overrideEdges.map(e => ({
      id: e.id,
      from: e.source,
      to: e.target,
      condition: (e.label as string) === '—' ? '' : String(e.label ?? ''),
    }));
    const initial = overrideNodes.find(n => n.data.isInitial)?.id ?? null;
    onChange({ initial, transitions, positions });
  }

  const onConnect = useCallback(
    (connection: Connection) => {
      const newEdge: Edge = {
        ...connection,
        id: uid(),
        source: connection.source!,
        target: connection.target!,
        label: '—',
        labelStyle: { fill: '#8aff80', fontSize: 10, fontFamily: 'monospace' },
        labelBgStyle: { fill: '#0d1a0d', fillOpacity: 0.85 },
        style: { stroke: '#4a8a4a' },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#4a8a4a' },
      };
      setEdges(es => {
        const next = addEdge(newEdge, es);
        emitChange(nodes, next);
        return next;
      });
    },
    [nodes, setEdges], // eslint-disable-line react-hooks/exhaustive-deps
  );

  function handleNodeDragStop(_: unknown, node: Node) {
    setNodes(ns => {
      const next = ns.map(n => n.id === node.id ? { ...n, position: node.position } : n);
      emitChange(next, edges);
      return next;
    });
  }

  function handleEdgeClick(_: unknown, edge: Edge) {
    setSelectedEdgeId(edge.id);
    setConditionEdit((edge.label as string) === '—' ? '' : String(edge.label ?? ''));
  }

  function handlePaneClick() {
    setSelectedEdgeId(null);
  }

  function saveEdgeCondition() {
    setEdges(es => {
      const next = es.map(e =>
        e.id === selectedEdgeId
          ? { ...e, label: conditionEdit.trim() || '—' }
          : e,
      );
      emitChange(nodes, next);
      return next;
    });
    setSelectedEdgeId(null);
  }

  function deleteEdge(id: string) {
    setEdges(es => {
      const next = es.filter(e => e.id !== id);
      emitChange(nodes, next);
      return next;
    });
    setSelectedEdgeId(null);
  }

  function addTransition() {
    if (!addFrom || !addTo || addFrom === addTo) return;
    const newEdge: Edge = {
      id: uid(),
      source: addFrom,
      target: addTo,
      label: addCondition.trim() || '—',
      labelStyle: { fill: '#8aff80', fontSize: 10, fontFamily: 'monospace' },
      labelBgStyle: { fill: '#0d1a0d', fillOpacity: 0.85 },
      style: { stroke: '#4a8a4a' },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#4a8a4a' },
    };
    setEdges(es => {
      const next = [...es, newEdge];
      emitChange(nodes, next);
      return next;
    });
  }

  function setInitial(name: string) {
    setInitialEdit(name);
    setNodes(ns => {
      const next = ns.map(n => ({ ...n, data: { ...n.data, isInitial: n.id === name } }));
      emitChange(next, edges);
      return next;
    });
  }

  function autoLayout() {
    setNodes(ns => {
      const next = ns.map((n, i) => ({
        ...n,
        position: { x: (i % 4) * GRID_COL, y: Math.floor(i / 4) * GRID_ROW },
      }));
      emitChange(next, edges);
      return next;
    });
  }

  if (seqNames.length === 0) {
    return (
      <div className="flex-1 flex items-center justify-center text-game-textDim text-sm">
        Add sequences in the Animation tab first.
      </div>
    );
  }

  return (
    <div className="flex flex-1 min-h-0">
      {/* Canvas */}
      <div className="flex-1 min-h-0" style={{ background: '#050a05' }}>
        <ReactFlow
          nodes={nodes}
          edges={edges}
          nodeTypes={NODE_TYPES}
          onNodesChange={onNodesChange}
          onEdgesChange={onEdgesChange}
          onConnect={onConnect}
          onNodeDragStop={handleNodeDragStop}
          onEdgeClick={handleEdgeClick}
          onPaneClick={handlePaneClick}
          fitView
          deleteKeyCode={['Backspace', 'Delete']}
          proOptions={{ hideAttribution: true }}
        >
          <Background variant={BackgroundVariant.Dots} color="#1a2e1a" gap={20} />
          <Controls style={{ background: '#0d1a0d', border: '1px solid #2a4a2a' }} />
          <MiniMap
            nodeColor={n => n.data?.isInitial ? '#8aff80' : '#1a3a1a'}
            maskColor="rgba(5,10,5,0.8)"
            style={{ background: '#0d1a0d', border: '1px solid #2a4a2a' }}
          />
        </ReactFlow>
      </div>

      {/* Right inspector */}
      <div className="w-64 border-l border-game-border flex flex-col shrink-0 overflow-y-auto">

        {/* Initial state */}
        <section className="p-3 border-b border-game-border">
          <div className="text-[10px] text-game-textDim tracking-widest mb-2">INITIAL STATE</div>
          <select
            value={initialEdit}
            onChange={e => setInitial(e.target.value)}
            className="w-full bg-game-bgCard border border-game-border text-game-text text-xs font-mono px-2 py-1 focus:outline-none focus:border-game-primary"
          >
            {seqNames.map(n => <option key={n} value={n}>{n}</option>)}
          </select>
        </section>

        {/* Add transition */}
        <section className="p-3 border-b border-game-border space-y-2">
          <div className="text-[10px] text-game-textDim tracking-widest">ADD TRANSITION</div>
          <select
            value={addFrom}
            onChange={e => setAddFrom(e.target.value)}
            className="w-full bg-game-bgCard border border-game-border text-game-text text-xs font-mono px-2 py-1 focus:outline-none focus:border-game-primary"
          >
            {seqNames.map(n => <option key={n} value={n}>{n}</option>)}
          </select>
          <div className="text-[10px] text-game-textDim text-center">→</div>
          <select
            value={addTo}
            onChange={e => setAddTo(e.target.value)}
            className="w-full bg-game-bgCard border border-game-border text-game-text text-xs font-mono px-2 py-1 focus:outline-none focus:border-game-primary"
          >
            {seqNames.map(n => <option key={n} value={n}>{n}</option>)}
          </select>
          <input
            list="conditions"
            value={addCondition}
            onChange={e => setAddCondition(e.target.value)}
            placeholder="condition…"
            className="w-full bg-game-bgCard border border-game-border text-game-text text-xs font-mono px-2 py-1 focus:outline-none focus:border-game-primary"
          />
          <datalist id="conditions">
            {KNOWN_CONDITIONS.map(c => <option key={c} value={c} />)}
          </datalist>
          <button
            onClick={addTransition}
            disabled={!addFrom || !addTo || addFrom === addTo}
            className="w-full py-1.5 text-xs font-mono border border-game-border hover:border-game-primary text-game-textDim hover:text-game-text disabled:opacity-30 transition-colors"
          >
            + ADD
          </button>
          <div className="text-[9px] text-game-textDim">
            Or drag from a node handle to another node.
          </div>
        </section>

        {/* Edge inspector */}
        {selectedEdge && (
          <section className="p-3 border-b border-game-border space-y-2">
            <div className="text-[10px] text-game-textDim tracking-widest">SELECTED TRANSITION</div>
            <div className="text-xs font-mono text-game-textDim">
              {selectedEdge.source} → {selectedEdge.target}
            </div>
            <input
              list="conditions"
              value={conditionEdit}
              onChange={e => setConditionEdit(e.target.value)}
              placeholder="condition…"
              className="w-full bg-game-bgCard border border-game-border text-game-text text-xs font-mono px-2 py-1 focus:outline-none focus:border-game-primary"
            />
            <div className="flex gap-1">
              <button
                onClick={saveEdgeCondition}
                className="flex-1 py-1 text-xs font-mono border border-game-primary text-game-primary hover:bg-game-primary/10 transition-colors"
              >
                SAVE
              </button>
              <button
                onClick={() => deleteEdge(selectedEdge.id)}
                className="flex-1 py-1 text-xs font-mono border border-red-900 text-red-400 hover:bg-red-900/20 transition-colors"
              >
                DELETE
              </button>
            </div>
          </section>
        )}

        {/* Layout */}
        <section className="p-3 mt-auto">
          <button
            onClick={autoLayout}
            className="w-full py-1.5 text-xs font-mono border border-game-border hover:border-game-primary text-game-textDim hover:text-game-text transition-colors"
          >
            AUTO LAYOUT
          </button>
        </section>
      </div>
    </div>
  );
}

export default function StateMachineTab(props: Props) {
  return (
    <ReactFlowProvider>
      <StateMachineInner {...props} />
    </ReactFlowProvider>
  );
}
