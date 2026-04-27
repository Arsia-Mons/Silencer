'use client';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import ReactFlow, {
  addEdge,
  Background,
  Controls,
  Handle,
  Position,
  useEdgesState,
  useNodesState,
  type Connection,
  type Edge,
  type Node,
  type NodeProps,
} from 'reactflow';
import dagre from 'dagre';
import type { BehaviorTree, BTNode, BTNodeType, BBKey } from '../../../lib/api';

// ── Node colours by type ──────────────────────────────────────────────────────
const TYPE_COLOR: Record<string, string> = {
  Selector:       '#f59e0b', // amber
  Sequence:       '#3b82f6', // blue
  Parallel:       '#8b5cf6', // purple
  RandomSelector: '#f59e0b', // amber (lighter)
  Inverter:       '#ec4899', // pink
  Cooldown:       '#06b6d4', // cyan
  Repeat:         '#10b981', // emerald
  Timeout:        '#ef4444', // red
  ForceSuccess:   '#84cc16', // lime
  Wait:           '#94a3b8', // slate
  Leaf:           '#22c55e', // green
  Condition:      '#f97316', // orange
};

const TYPE_SYMBOL: Record<string, string> = {
  Selector:       '?',
  Sequence:       '→',
  Parallel:       '⇉',
  RandomSelector: '⁈',
  Inverter:       '¬',
  Cooldown:       '⏱',
  Repeat:         '↻',
  Timeout:        '⏰',
  ForceSuccess:   '✓',
  Wait:           '◷',
  Leaf:           '▶',
  Condition:      '◆',
};

// ── Custom node ───────────────────────────────────────────────────────────────
function BTNodeComponent({ data, selected }: NodeProps) {
  const color = TYPE_COLOR[data.type] ?? '#6b7280';
  return (
    <>
      <style>{`
        @keyframes btPulse {
          0%,100% { box-shadow: 0 0 0 0 ${color}66; }
          50%      { box-shadow: 0 0 0 6px ${color}00; }
        }
      `}</style>
      <Handle type="target" position={Position.Top} style={{ background: color, border: 'none', width: 8, height: 8 }} />
      <div style={{
        background: '#0d1117',
        border: `2px solid ${selected ? color : '#2d3748'}`,
        borderRadius: 6,
        padding: '6px 12px',
        minWidth: 120,
        textAlign: 'center',
        cursor: 'pointer',
        animation: selected ? 'btPulse 1.2s ease-in-out infinite' : 'none',
        transition: 'border-color 0.15s',
      }}>
        <div style={{ color, fontSize: 11, fontWeight: 700, letterSpacing: 2, marginBottom: 2 }}>
          {TYPE_SYMBOL[data.type]} {data.type.toUpperCase()}
        </div>
        <div style={{ color: '#e2e8f0', fontSize: 12, fontFamily: 'monospace' }}>{data.label}</div>
        {data.subtitle && (
          <div style={{ color: '#718096', fontSize: 10, marginTop: 2 }}>{data.subtitle}</div>
        )}
      </div>
      <Handle type="source" position={Position.Bottom} style={{ background: color, border: 'none', width: 8, height: 8 }} />
    </>
  );
}

const NODE_TYPES = { btNode: BTNodeComponent };

// ── Dagre layout (top-down) ───────────────────────────────────────────────────
function applyDagre(nodes: Node[], edges: Edge[]): Node[] {
  const g = new dagre.graphlib.Graph();
  g.setDefaultEdgeLabel(() => ({}));
  g.setGraph({ rankdir: 'TB', ranksep: 70, nodesep: 40 });
  nodes.forEach(n => g.setNode(n.id, { width: 140, height: 60 }));
  edges.forEach(e => g.setEdge(e.source, e.target));
  dagre.layout(g);
  return nodes.map(n => {
    const { x, y } = g.node(n.id);
    return { ...n, position: { x: x - 70, y: y - 30 } };
  });
}

// ── Helpers ───────────────────────────────────────────────────────────────────
function uid() {
  return Math.random().toString(36).slice(2, 9);
}

function nodeSubtitle(node: BTNode): string {
  if (node.type === 'Condition') {
    const p = node.props;
    return `${p.key} ${p.op} ${JSON.stringify(p.value)}`;
  }
  if (node.type === 'Leaf') return String(node.props.action ?? '');
  if (node.type === 'Cooldown') return `${node.props.duration ?? '?'}s`;
  if (node.type === 'Repeat') return `×${node.props.count ?? '∞'}`;
  if (node.type === 'Timeout') return `timeout ${node.props.duration ?? '?'}s`;
  if (node.type === 'Wait') return `wait ${node.props.duration ?? '?'}s`;
  return '';
}

function btToFlow(bt: BehaviorTree): { nodes: Node[]; edges: Edge[] } {
  const nodes: Node[] = Object.entries(bt.nodes).map(([id, node]) => ({
    id,
    type: 'btNode',
    position: bt.positions?.[id] ?? { x: 0, y: 0 },
    data: { type: node.type, label: node.label, subtitle: nodeSubtitle(node) },
  }));

  const edges: Edge[] = [];
  Object.entries(bt.nodes).forEach(([parentId, node]) => {
    node.children.forEach((childId, idx) => {
      edges.push({
        id: `${parentId}-${childId}`,
        source: parentId,
        target: childId,
        label: String(idx + 1),
        type: 'smoothstep',
        style: { stroke: '#4a5568', strokeWidth: 2 },
        labelStyle: { fill: '#718096', fontSize: 10 },
        labelBgStyle: { fill: '#0d1117' },
      });
    });
  });

  const hasPositions = Object.keys(bt.positions ?? {}).length > 0;
  if (!hasPositions) return { nodes: applyDagre(nodes, edges), edges };
  return { nodes, edges };
}

// ── Node palette config ───────────────────────────────────────────────────────
const PALETTE: { group: string; types: BTNodeType[] }[] = [
  { group: 'COMPOSITE', types: ['Selector', 'Sequence', 'Parallel', 'RandomSelector'] },
  { group: 'DECORATOR', types: ['Inverter', 'Cooldown', 'Repeat', 'Timeout', 'ForceSuccess'] },
  { group: 'LEAF',      types: ['Leaf', 'Condition', 'Wait'] },
];

const LEAF_ACTIONS = [
  'Patrol', 'Stand', 'Look',
  'ShootStanding', 'ShootCrouched', 'ShootUp', 'ShootDown', 'ShootUpAngle', 'ShootDownAngle',
  'Crouch', 'Uncrouch',
  'ClimbLadder', 'Alert', 'Melee',
  'Run', 'Wander', 'Sleep', 'Idle',
];

// ── Main editor ───────────────────────────────────────────────────────────────
interface Props { bt: BehaviorTree; onChange: (bt: BehaviorTree) => void; }

export default function BehaviorTreeEditor({ bt, onChange }: Props) {
  const [rfNodes, setRfNodes, onNodesChange] = useNodesState([]);
  const [rfEdges, setRfEdges, onEdgesChange] = useEdgesState([]);
  const [selectedNodeId, setSelectedNodeId] = useState<string | null>(null);
  const [validateMsg, setValidateMsg] = useState<string | null>(null);
  const btRef = useRef(bt);
  btRef.current = bt;

  // Sync BT → ReactFlow
  useEffect(() => {
    const { nodes, edges } = btToFlow(bt);
    setRfNodes(nodes);
    setRfEdges(edges);
  }, [bt.id]); // eslint-disable-line react-hooks/exhaustive-deps

  const onConnect = useCallback((params: Connection) => {
    if (!params.source || !params.target) return;
    const cur = btRef.current;
    const parentNode = cur.nodes[params.source];
    if (!parentNode) return;
    if (parentNode.children.includes(params.target)) return; // already connected

    const isDecorator = ['Inverter', 'Cooldown', 'Repeat', 'Timeout', 'ForceSuccess'].includes(parentNode.type);
    if (isDecorator && parentNode.children.length >= 1) return;

    const updated: BehaviorTree = {
      ...cur,
      nodes: {
        ...cur.nodes,
        [params.source]: { ...parentNode, children: [...parentNode.children, params.target] },
      },
    };
    onChange(updated);
    setRfEdges(es => addEdge({ ...params, type: 'smoothstep', style: { stroke: '#4a5568', strokeWidth: 2 } }, es));
  }, [onChange]);

  function autoLayout() {
    const laid = applyDagre(rfNodes, rfEdges);
    setRfNodes(laid);
    const positions: Record<string, { x: number; y: number }> = {};
    laid.forEach(n => { positions[n.id] = n.position; });
    onChange({ ...btRef.current, positions });
  }

  function validate(): string | null {
    const cur = btRef.current;
    if (!cur.nodes[cur.rootId]) return `Root node "${cur.rootId}" missing`;
    const parentCount: Record<string, number> = {};
    for (const [pid, node] of Object.entries(cur.nodes)) {
      const isDecorator = ['Inverter', 'Cooldown', 'Repeat', 'Timeout', 'ForceSuccess'].includes(node.type);
      if (isDecorator && node.children.length !== 1) return `Decorator "${pid}" must have exactly 1 child`;
      const isLeaf = ['Leaf', 'Condition', 'Wait'].includes(node.type);
      if (isLeaf && node.children.length > 0) return `${node.type} "${pid}" must have 0 children`;
      for (const cid of node.children) {
        parentCount[cid] = (parentCount[cid] ?? 0) + 1;
        if (parentCount[cid] > 1) return `Node "${cid}" has multiple parents`;
        if (!cur.nodes[cid]) return `Node "${pid}" references unknown child "${cid}"`;
      }
    }
    // Cycle check
    const visited = new Set<string>();
    function dfs(id: string): string | null {
      if (visited.has(id)) return `Cycle at "${id}"`;
      visited.add(id);
      for (const c of cur.nodes[id]?.children ?? []) {
        const e = dfs(c); if (e) return e;
      }
      visited.delete(id);
      return null;
    }
    return dfs(cur.rootId);
  }

  function addNode(type: BTNodeType) {
    const id = `${type.toLowerCase()}_${uid()}`;
    const node: BTNode = { type, label: type, children: [], props: {} };
    onChange({ ...btRef.current, nodes: { ...btRef.current.nodes, [id]: node } });
    setRfNodes(ns => [...ns, {
      id, type: 'btNode',
      position: { x: 100 + Math.random() * 200, y: 100 + Math.random() * 200 },
      data: { type, label: type, subtitle: '' },
    }]);
  }

  function deleteSelectedNode() {
    if (!selectedNodeId) return;
    const cur = btRef.current;
    if (selectedNodeId === cur.rootId) return;
    const nodes = { ...cur.nodes };
    delete nodes[selectedNodeId];
    // Remove from all children arrays
    for (const n of Object.values(nodes)) {
      n.children = n.children.filter((c: string) => c !== selectedNodeId);
    }
    onChange({ ...cur, nodes });
    setRfNodes(ns => ns.filter(n => n.id !== selectedNodeId));
    setRfEdges(es => es.filter(e => e.source !== selectedNodeId && e.target !== selectedNodeId));
    setSelectedNodeId(null);
  }

  const selectedNode = selectedNodeId ? bt.nodes[selectedNodeId] : null;

  function updateSelectedNode(patch: Partial<BTNode>) {
    if (!selectedNodeId) return;
    const cur = btRef.current;
    const updated: BehaviorTree = {
      ...cur,
      nodes: { ...cur.nodes, [selectedNodeId]: { ...cur.nodes[selectedNodeId], ...patch } },
    };
    onChange(updated);
    setRfNodes(ns => ns.map(n => n.id === selectedNodeId
      ? { ...n, data: { ...n.data, label: patch.label ?? n.data.label, subtitle: nodeSubtitle(updated.nodes[selectedNodeId]) } }
      : n));
  }

  function updateProp(key: string, value: unknown) {
    if (!selectedNodeId) return;
    const cur = btRef.current;
    const node = cur.nodes[selectedNodeId];
    updateSelectedNode({ props: { ...node.props, [key]: value } });
  }

  function addBBKey() {
    const cur = btRef.current;
    // Auto-generate a unique key name
    let n = cur.blackboard.length + 1;
    while (cur.blackboard.some(k => k.key === `key_${n}`)) n++;
    onChange({ ...cur, blackboard: [...cur.blackboard, { key: `key_${n}`, type: 'bool', default: false }] });
  }

  function updateBBKey(idx: number, patch: Partial<BBKey>) {
    const bb = [...bt.blackboard];
    // When type changes, reset default to a sensible value
    if (patch.type && patch.type !== bb[idx].type) {
      patch.default = patch.type === 'bool' ? false : patch.type === 'string' ? '' : 0;
    }
    bb[idx] = { ...bb[idx], ...patch };
    onChange({ ...bt, blackboard: bb });
  }

  function removeBBKey(idx: number) {
    onChange({ ...bt, blackboard: bt.blackboard.filter((_, i) => i !== idx) });
  }

  // How many Condition nodes reference a given blackboard key
  function bbUsedBy(key: string): number {
    return Object.values(bt.nodes).filter(n => n.type === 'Condition' && n.props.key === key).length;
  }

  const BB_TYPE_COLOR: Record<string, string> = {
    bool:   '#22c55e',
    int:    '#3b82f6',
    float:  '#8b5cf6',
    string: '#f59e0b',
  };

  // Duplicate key names
  const bbKeyNames = bt.blackboard.map(k => k.key);
  const bbDupes = new Set(bbKeyNames.filter((k, i) => bbKeyNames.indexOf(k) !== i));

  return (
    <div style={{ display: 'flex', height: '100%', background: '#0d1117' }}>
      {/* Left palette */}
      <div style={{ width: 160, borderRight: '1px solid #2d3748', padding: '12px 8px', overflowY: 'auto', flexShrink: 0 }}>
        <div style={{ color: '#718096', fontSize: 10, letterSpacing: 2, marginBottom: 8 }}>PALETTE</div>
        {PALETTE.map(({ group, types }) => (
          <div key={group} style={{ marginBottom: 12 }}>
            <div style={{ color: '#4a5568', fontSize: 9, letterSpacing: 2, marginBottom: 4 }}>{group}</div>
            {types.map(t => (
              <button key={t} onClick={() => addNode(t)}
                style={{
                  display: 'block', width: '100%', marginBottom: 4, padding: '5px 8px',
                  background: '#161b22', border: `1px solid ${TYPE_COLOR[t]}44`,
                  color: TYPE_COLOR[t], fontSize: 11, fontFamily: 'monospace',
                  cursor: 'pointer', textAlign: 'left', letterSpacing: 1,
                }}
                onMouseEnter={e => { (e.currentTarget as HTMLButtonElement).style.background = `${TYPE_COLOR[t]}22`; }}
                onMouseLeave={e => { (e.currentTarget as HTMLButtonElement).style.background = '#161b22'; }}
              >
                {TYPE_SYMBOL[t]} {t}
              </button>
            ))}
          </div>
        ))}
        <div style={{ borderTop: '1px solid #2d3748', paddingTop: 8, marginTop: 4 }}>
          <button onClick={autoLayout}
            style={{ display: 'block', width: '100%', padding: '5px 8px', background: '#161b22', border: '1px solid #2d3748', color: '#a0aec0', fontSize: 10, fontFamily: 'monospace', cursor: 'pointer', letterSpacing: 1, marginBottom: 4 }}>
            AUTO LAYOUT
          </button>
          <button
            onClick={() => { const e = validate(); setValidateMsg(e ?? '✓ Valid'); }}
            style={{ display: 'block', width: '100%', padding: '5px 8px', background: '#161b22', border: '1px solid #2d3748', color: '#a0aec0', fontSize: 10, fontFamily: 'monospace', cursor: 'pointer', letterSpacing: 1 }}>
            VALIDATE
          </button>
          {validateMsg && (
            <div style={{ marginTop: 6, fontSize: 10, color: validateMsg.startsWith('✓') ? '#22c55e' : '#f87171', wordBreak: 'break-word' }}>
              {validateMsg}
            </div>
          )}
          {selectedNodeId && selectedNodeId !== bt.rootId && (
            <button onClick={deleteSelectedNode}
              style={{ display: 'block', width: '100%', marginTop: 8, padding: '5px 8px', background: '#1a0000', border: '1px solid #7f1d1d', color: '#f87171', fontSize: 10, fontFamily: 'monospace', cursor: 'pointer', letterSpacing: 1 }}>
              DELETE NODE
            </button>
          )}
        </div>
      </div>

      {/* Canvas */}
      <div style={{ flex: 1, position: 'relative' }}>
        <ReactFlow
          nodes={rfNodes}
          edges={rfEdges}
          onNodesChange={onNodesChange}
          onEdgesChange={onEdgesChange}
          onConnect={onConnect}
          nodeTypes={NODE_TYPES}
          onNodeClick={(_, n) => setSelectedNodeId(n.id)}
          onPaneClick={() => setSelectedNodeId(null)}
          onNodeDragStop={(_, n) => {
            onChange({ ...btRef.current, positions: { ...btRef.current.positions, [n.id]: n.position } });
          }}
          fitView
        >
          <Background color="#1a1f2e" gap={20} />
          <Controls style={{ background: '#161b22', border: '1px solid #2d3748' }} />
        </ReactFlow>
      </div>

      {/* Right panel */}
      <div style={{ width: 220, borderLeft: '1px solid #2d3748', overflowY: 'auto', flexShrink: 0 }}>
        {/* Node config */}
        {selectedNode ? (
          <div style={{ padding: '12px 10px', borderBottom: '1px solid #2d3748' }}>
            <div style={{ color: TYPE_COLOR[selectedNode.type], fontSize: 10, letterSpacing: 2, marginBottom: 8 }}>
              {selectedNode.type.toUpperCase()}
            </div>
            <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>LABEL</label>
            <input value={selectedNode.label} onChange={e => updateSelectedNode({ label: e.target.value })}
              style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }} />

            {selectedNode.type === 'Leaf' && (
              <>
                <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>ACTION</label>
                <select value={String(selectedNode.props.action ?? '')} onChange={e => updateProp('action', e.target.value)}
                  style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }}>
                  {LEAF_ACTIONS.map(a => <option key={a} value={a}>{a}</option>)}
                </select>
              </>
            )}

            {selectedNode.type === 'Condition' && (
              <>
                <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>BLACKBOARD KEY</label>
                <select value={String(selectedNode.props.key ?? '')} onChange={e => updateProp('key', e.target.value)}
                  style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 4, boxSizing: 'border-box' }}>
                  <option value="">— select key —</option>
                  {bt.blackboard.map(k => <option key={k.key} value={k.key}>{k.key}</option>)}
                </select>
                <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>OPERATOR</label>
                <select value={String(selectedNode.props.op ?? '==')} onChange={e => updateProp('op', e.target.value)}
                  style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 4, boxSizing: 'border-box' }}>
                  {['==', '!=', '>', '<', '>=', '<='].map(op => <option key={op} value={op}>{op}</option>)}
                </select>
                <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>VALUE</label>
                {(() => {
                  const bbEntry = bt.blackboard.find(k => k.key === selectedNode.props.key);
                  const kType = bbEntry?.type ?? 'string';
                  if (kType === 'bool') {
                    const cur = selectedNode.props.value;
                    const isTrueish = cur === true || cur === 'true';
                    return (
                      <div style={{ display: 'flex', gap: 4, marginBottom: 8 }}>
                        {[true, false].map(v => (
                          <button key={String(v)} onClick={() => updateProp('value', v)}
                            style={{
                              flex: 1, padding: '4px 0', fontSize: 10, fontFamily: 'monospace', cursor: 'pointer',
                              background: (v ? isTrueish : !isTrueish) ? (v ? '#14532d' : '#450a0a') : '#161b22',
                              border: `1px solid ${(v ? isTrueish : !isTrueish) ? (v ? '#22c55e' : '#ef4444') : '#2d3748'}`,
                              color: (v ? isTrueish : !isTrueish) ? (v ? '#22c55e' : '#ef4444') : '#4a5568',
                            }}>
                            {String(v)}
                          </button>
                        ))}
                      </div>
                    );
                  }
                  if (kType === 'int') {
                    return <input type="number" step={1} value={Number(selectedNode.props.value ?? 0)}
                      onChange={e => updateProp('value', parseInt(e.target.value))}
                      style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }} />;
                  }
                  if (kType === 'float') {
                    return <input type="number" step={0.1} value={Number(selectedNode.props.value ?? 0)}
                      onChange={e => updateProp('value', parseFloat(e.target.value))}
                      style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }} />;
                  }
                  return <input value={String(selectedNode.props.value ?? '')}
                    onChange={e => updateProp('value', e.target.value)}
                    style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }} />;
                })()}
              </>
            )}

            {selectedNode.type === 'Cooldown' && (
              <>
                <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>DURATION (s)</label>
                <input type="number" value={Number(selectedNode.props.duration ?? 1)} min={0.1} step={0.1}
                  onChange={e => updateProp('duration', parseFloat(e.target.value))}
                  style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }} />
              </>
            )}

            {(selectedNode.type === 'Timeout' || selectedNode.type === 'Wait') && (
              <>
                <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>DURATION (s)</label>
                <input type="number" value={Number(selectedNode.props.duration ?? 1)} min={0.1} step={0.1}
                  onChange={e => updateProp('duration', parseFloat(e.target.value))}
                  style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }} />
              </>
            )}

            {selectedNode.type === 'Repeat' && (
              <>
                <label style={{ color: '#718096', fontSize: 10, display: 'block', marginBottom: 2 }}>COUNT (0=∞)</label>
                <input type="number" value={Number(selectedNode.props.count ?? 0)} min={0} step={1}
                  onChange={e => updateProp('count', parseInt(e.target.value))}
                  style={{ width: '100%', background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '4px 6px', fontSize: 11, fontFamily: 'monospace', marginBottom: 8, boxSizing: 'border-box' }} />
              </>
            )}

            <div style={{ color: '#4a5568', fontSize: 10, letterSpacing: 1 }}>
              ID: {selectedNodeId}
              {selectedNodeId === bt.rootId && ' (root)'}
            </div>
          </div>
        ) : (
          <div style={{ padding: '12px 10px', borderBottom: '1px solid #2d3748', color: '#4a5568', fontSize: 10 }}>
            Click a node to edit
          </div>
        )}

        {/* Blackboard editor */}
        <div style={{ padding: '12px 10px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
            <div style={{ color: '#718096', fontSize: 10, letterSpacing: 2 }}>
              BLACKBOARD
              {bt.blackboard.length > 0 && (
                <span style={{ marginLeft: 6, color: '#4a5568' }}>({bt.blackboard.length})</span>
              )}
            </div>
            <button onClick={addBBKey}
              style={{ background: 'none', border: '1px solid #2d3748', color: '#a0aec0', fontSize: 10, padding: '2px 8px', cursor: 'pointer', fontFamily: 'monospace', letterSpacing: 1 }}>
              + KEY
            </button>
          </div>
          {bt.blackboard.length === 0 && (
            <div style={{ color: '#4a5568', fontSize: 10, padding: '8px 0' }}>No keys — add one above</div>
          )}
          {bt.blackboard.map((k, i) => {
            const typeColor = BB_TYPE_COLOR[k.type] ?? '#718096';
            const isDupe = bbDupes.has(k.key);
            const usedBy = bbUsedBy(k.key);
            return (
              <div key={i} style={{
                marginBottom: 8, padding: '8px', background: '#0d1117',
                border: `1px solid ${isDupe ? '#ef4444' : '#2d3748'}`,
                borderLeft: `3px solid ${typeColor}`,
              }}>
                {/* Row 1: key name + delete */}
                <div style={{ display: 'flex', alignItems: 'center', gap: 4, marginBottom: 6 }}>
                  <input
                    value={k.key}
                    onChange={e => updateBBKey(i, { key: e.target.value.toLowerCase().replace(/[^a-z0-9_]/g, '_') })}
                    placeholder="key_name"
                    style={{
                      flex: 1, background: 'none', border: 'none', borderBottom: `1px solid ${isDupe ? '#ef4444' : '#2d3748'}`,
                      color: isDupe ? '#ef4444' : '#e2e8f0', fontSize: 12, fontFamily: 'monospace',
                      padding: '1px 0', minWidth: 0, outline: 'none',
                    }}
                  />
                  <button onClick={() => removeBBKey(i)}
                    style={{ background: 'none', border: 'none', color: '#4a5568', cursor: 'pointer', fontSize: 14, padding: '0 2px', lineHeight: 1, flexShrink: 0 }}
                    title="Remove key">✕</button>
                </div>
                {isDupe && (
                  <div style={{ color: '#ef4444', fontSize: 9, marginBottom: 4 }}>⚠ duplicate key name</div>
                )}
                {/* Row 2: type select */}
                <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 6 }}>
                  <span style={{ color: '#4a5568', fontSize: 9, letterSpacing: 1, flexShrink: 0 }}>TYPE</span>
                  <div style={{ display: 'flex', gap: 2, flex: 1 }}>
                    {(['bool', 'int', 'float', 'string'] as BBKey['type'][]).map(t => (
                      <button key={t} onClick={() => updateBBKey(i, { type: t })}
                        style={{
                          flex: 1, padding: '2px 0', fontSize: 9, fontFamily: 'monospace', cursor: 'pointer',
                          background: k.type === t ? `${BB_TYPE_COLOR[t]}22` : 'transparent',
                          border: `1px solid ${k.type === t ? BB_TYPE_COLOR[t] : '#2d3748'}`,
                          color: k.type === t ? BB_TYPE_COLOR[t] : '#4a5568',
                          letterSpacing: 0,
                        }}>
                        {t}
                      </button>
                    ))}
                  </div>
                </div>
                {/* Row 3: default value */}
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                  <span style={{ color: '#4a5568', fontSize: 9, letterSpacing: 1, flexShrink: 0 }}>DEFAULT</span>
                  {k.type === 'bool' ? (
                    <div style={{ display: 'flex', gap: 3, flex: 1 }}>
                      {[true, false].map(v => (
                        <button key={String(v)} onClick={() => updateBBKey(i, { default: v })}
                          style={{
                            flex: 1, padding: '2px 0', fontSize: 9, fontFamily: 'monospace', cursor: 'pointer',
                            background: k.default === v ? (v ? '#14532d' : '#450a0a') : 'transparent',
                            border: `1px solid ${k.default === v ? (v ? '#22c55e' : '#ef4444') : '#2d3748'}`,
                            color: k.default === v ? (v ? '#22c55e' : '#f87171') : '#4a5568',
                          }}>
                          {String(v)}
                        </button>
                      ))}
                    </div>
                  ) : k.type === 'int' ? (
                    <input type="number" step={1} value={Number(k.default ?? 0)}
                      onChange={e => updateBBKey(i, { default: parseInt(e.target.value) })}
                      style={{ flex: 1, background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '2px 4px', fontSize: 10, fontFamily: 'monospace' }} />
                  ) : k.type === 'float' ? (
                    <input type="number" step={0.1} value={Number(k.default ?? 0)}
                      onChange={e => updateBBKey(i, { default: parseFloat(e.target.value) })}
                      style={{ flex: 1, background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '2px 4px', fontSize: 10, fontFamily: 'monospace' }} />
                  ) : (
                    <input value={String(k.default ?? '')}
                      onChange={e => updateBBKey(i, { default: e.target.value })}
                      style={{ flex: 1, background: '#161b22', border: '1px solid #2d3748', color: '#e2e8f0', padding: '2px 4px', fontSize: 10, fontFamily: 'monospace' }} />
                  )}
                </div>
                {/* Row 4: usage */}
                {usedBy > 0 && (
                  <div style={{ marginTop: 5, fontSize: 9, color: typeColor, letterSpacing: 0.5 }}>
                    ↳ used by {usedBy} condition{usedBy > 1 ? 's' : ''}
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
