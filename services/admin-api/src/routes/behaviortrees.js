/**
 * Behavior tree definition CRUD endpoints.
 *
 * GET    /behaviortrees         — list all behavior tree IDs
 * GET    /behaviortrees/:id     — return parsed behavior tree JSON
 * PUT    /behaviortrees/:id     — write behavior tree JSON (admin only)
 * DELETE /behaviortrees/:id     — delete behavior tree (admin only)
 *
 * Schema: { version, id, blackboard:[{key,type,default}], rootId, nodes:{id:{type,label,children[],props{}}}, positions:{} }
 */

import { Router } from 'express';
import { readdirSync, readFileSync, writeFileSync, unlinkSync, existsSync, mkdirSync } from 'fs';
import { join } from 'path';
import { requireAuth, requireRole } from '../auth/jwt.js';
import { ASSETS_DIR } from '../config.js';

const router = Router();
const BT_DIR = join(ASSETS_DIR, 'behaviortrees');

const VALID_NODE_TYPES = new Set([
  'Selector', 'Sequence', 'Parallel', 'RandomSelector',
  'Inverter', 'Cooldown', 'Repeat', 'Timeout', 'ForceSuccess',
  'Wait', 'Leaf', 'Condition',
]);

function btPath(id) {
  if (!id || /[/\\.]/.test(id)) throw new Error('Invalid behavior tree id');
  return join(BT_DIR, `${id}.json`);
}

function ensureDir() {
  if (!existsSync(BT_DIR)) mkdirSync(BT_DIR, { recursive: true });
}

function validate(bt) {
  if (!bt || typeof bt !== 'object') return 'Not an object';
  if (!bt.id || typeof bt.id !== 'string') return 'Missing id';
  if (!bt.rootId || typeof bt.rootId !== 'string') return 'Missing rootId';
  if (!bt.nodes || typeof bt.nodes !== 'object') return 'Missing nodes';
  if (!bt.nodes[bt.rootId]) return `rootId "${bt.rootId}" not found in nodes`;
  for (const [id, node] of Object.entries(bt.nodes)) {
    if (!VALID_NODE_TYPES.has(node.type)) return `Node "${id}" has invalid type "${node.type}"`;
    if (!Array.isArray(node.children)) return `Node "${id}" missing children array`;
    const isDecorator = ['Inverter', 'Cooldown', 'Repeat', 'Timeout', 'ForceSuccess'].includes(node.type);
    if (isDecorator && node.children.length !== 1) return `Decorator "${id}" must have exactly 1 child`;
    const isLeafOrCond = ['Leaf', 'Condition', 'Wait'].includes(node.type);
    if (isLeafOrCond && node.children.length !== 0) return `${node.type} "${id}" must have 0 children`;
    for (const cid of node.children) {
      if (!bt.nodes[cid]) return `Node "${id}" references unknown child "${cid}"`;
    }
  }
  // Detect cycles via DFS
  const visited = new Set();
  function dfs(nid) {
    if (visited.has(nid)) return `Cycle detected at node "${nid}"`;
    visited.add(nid);
    for (const cid of bt.nodes[nid]?.children ?? []) {
      const err = dfs(cid);
      if (err) return err;
    }
    visited.delete(nid);
    return null;
  }
  return dfs(bt.rootId);
}

// GET /behaviortrees
router.get('/', requireAuth, (_req, res) => {
  ensureDir();
  try {
    const ids = readdirSync(BT_DIR)
      .filter(f => f.endsWith('.json'))
      .map(f => f.replace(/\.json$/, ''));
    res.json(ids);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /behaviortrees/:id
router.get('/:id', requireAuth, (req, res) => {
  try {
    const raw = readFileSync(btPath(req.params.id), 'utf8');
    res.json(JSON.parse(raw));
  } catch (e) {
    if (e.code === 'ENOENT') return res.status(404).json({ error: 'Not found' });
    res.status(500).json({ error: e.message });
  }
});

// PUT /behaviortrees/:id
router.put('/:id', requireAuth, requireRole('admin'), (req, res) => {
  ensureDir();
  try {
    const bt = req.body;
    const err = validate(bt);
    if (err) return res.status(400).json({ error: err });
    bt.id = req.params.id;
    writeFileSync(btPath(req.params.id), JSON.stringify(bt, null, 2));
    res.json({ ok: true });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// DELETE /behaviortrees/:id
router.delete('/:id', requireAuth, requireRole('admin'), (req, res) => {
  try {
    unlinkSync(btPath(req.params.id));
    res.json({ ok: true });
  } catch (e) {
    if (e.code === 'ENOENT') return res.status(404).json({ error: 'Not found' });
    res.status(500).json({ error: e.message });
  }
});

export default router;
