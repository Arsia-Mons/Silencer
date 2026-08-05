/**
 * Launcher endpoints — every HTTP request clients/launcher makes.
 *
 * GET /launcher/manifest/:channel — update.json for `stable` | `nightly`,
 *                                   or `self` for the launcher's own build
 * GET /launcher/news              — shared/news v2 block-AST feed
 * GET /launcher/releases          — GitHub Releases
 *
 * All public: the launcher runs before any lobby session exists, so there is
 * no token to present. GET only — nothing here writes.
 *
 * Two rules, identical locally and in production — so what you test is what
 * ships:
 *
 *   1. a file in LAUNCHER_DIR — always wins. Production's deploy writes the
 *      real manifests there; locally you edit dev-data/launcher freely.
 *   2. an upstream URL, proxied and cached — news and releases only. A manifest
 *      has no upstream: if nothing is published on that channel, that's a 404.
 */

import { Router } from 'express';
import { existsSync, readFileSync } from 'fs';
import { join } from 'path';
import {
  LAUNCHER_DIR,
  LAUNCHER_NEWS_URL,
  LAUNCHER_RELEASES_URL,
  GITHUB_TOKEN,
} from '../config.js';

const router = Router();

const CHANNELS = ['stable', 'nightly'];
// `self` is the launcher's own manifest, which it polls to update itself. It is
// not a game channel — the launcher ships on one track — but it resolves by the
// same rule, so it shares the handler rather than duplicating it.
const MANIFESTS = [...CHANNELS, 'self'];
const PROXY_TTL_MS = 5 * 60 * 1000;

const proxyCache = new Map(); // url -> { at, body }

// Returns null when the file is absent — a missing manifest is a normal state
// (nothing published on that channel yet), not an error.
function readJsonFile(path) {
  if (!existsSync(path)) return null;
  return JSON.parse(readFileSync(path, 'utf8'));
}

function localOverride(name, label) {
  try {
    return readJsonFile(join(LAUNCHER_DIR, name));
  } catch (err) {
    // A broken local override shouldn't be fatal — fall through to upstream.
    console.error(`[launcher] ${label} override unreadable:`, err.message);
    return null;
  }
}

// Proxies `url`, caching the parsed body for PROXY_TTL_MS. On upstream failure
// a stale entry is served if we have one: the launcher renders these as lists,
// and a list a few hours old still beats an error panel.
async function proxyJson(url, res, label) {
  const hit = proxyCache.get(url);
  if (hit && Date.now() - hit.at < PROXY_TTL_MS) return res.json(hit.body);

  const headers = { 'User-Agent': 'silencer-admin-api', Accept: 'application/json' };
  if (GITHUB_TOKEN && url.startsWith('https://api.github.com/')) {
    headers.Authorization = `token ${GITHUB_TOKEN}`;
  }

  try {
    const upstream = await fetch(url, { headers });
    if (!upstream.ok) throw new Error(`upstream HTTP ${upstream.status}`);
    const body = await upstream.json();
    proxyCache.set(url, { at: Date.now(), body });
    res.json(body);
  } catch (err) {
    console.error(`[launcher] ${label} upstream failed:`, err.message);
    if (hit) return res.json(hit.body);
    res.status(502).json({ error: `${label} upstream unavailable` });
  }
}

// GET /launcher/manifest/:channel — public. `stable` | `nightly` | `self`.
router.get('/manifest/:channel', (req, res) => {
  const { channel } = req.params;
  if (!MANIFESTS.includes(channel)) {
    return res.status(400).json({ error: `Unknown manifest "${channel}"` });
  }
  try {
    const manifest = readJsonFile(join(LAUNCHER_DIR, `manifest-${channel}.json`));
    if (!manifest) {
      return res.status(404).json({ error: `No ${channel} manifest published` });
    }
    res.json(manifest);
  } catch (err) {
    console.error(`[launcher] manifest ${channel} unreadable:`, err.message);
    res.status(500).json({ error: 'Manifest unreadable' });
  }
});

// GET /launcher/news — public
router.get('/news', async (_req, res) => {
  const override = localOverride('announcements.json', 'news');
  if (override) return res.json(override);
  await proxyJson(LAUNCHER_NEWS_URL, res, 'news');
});

// GET /launcher/releases — public. Proxied so the launcher fleet spends this
// server's rate-limit budget once per TTL instead of one call per client.
router.get('/releases', async (_req, res) => {
  const override = localOverride('releases.json', 'releases');
  if (override) return res.json(override);
  await proxyJson(LAUNCHER_RELEASES_URL, res, 'releases');
});

export default router;
