/**
 * Sound file endpoints.
 *
 * Filesystem is the source of truth. Sound files (WAV/OGG/MP3) live in
 * shared/assets/sounds/. sound-events.json in the same directory maps
 * event names → sound filenames.
 *
 * GET    /sounds                   — list sound files with metadata
 * GET    /sounds/:filename         — stream audio file for browser playback
 * POST   /sounds                   — upload a WAV/OGG/MP3 file (admin only)
 * DELETE /sounds/:filename         — delete a sound file (admin only)
 * GET    /sounds/events            — get sound-events.json mapping
 * PATCH  /sounds/events/:event     — update one event assignment (admin only)
 */

import { Router } from 'express';
import { existsSync, mkdirSync, readdirSync, statSync, unlinkSync, readFileSync, writeFileSync } from 'fs';
import { createReadStream } from 'fs';
import { join, extname, basename } from 'path';
import { requireAuth, requireRole } from '../auth/jwt.js';
import { ASSETS_DIR } from '../config.js';

const router = Router();
const SOUNDS_DIR = join(ASSETS_DIR, 'sounds');
const EVENTS_FILE = join(SOUNDS_DIR, 'sound-events.json');

const ALLOWED_EXTS = new Set(['.wav', '.ogg', '.mp3']);

const MIME = { '.wav': 'audio/wav', '.ogg': 'audio/ogg', '.mp3': 'audio/mpeg' };

function ensureSoundsDir() {
  if (!existsSync(SOUNDS_DIR)) mkdirSync(SOUNDS_DIR, { recursive: true });
}

function safeFilename(name) {
  if (!name || /[/\\]/.test(name) || name.startsWith('.')) throw new Error('Invalid filename');
  const ext = extname(name).toLowerCase();
  if (!ALLOWED_EXTS.has(ext)) throw new Error('Unsupported file type');
  return name;
}

function readEvents() {
  if (!existsSync(EVENTS_FILE)) return {};
  try { return JSON.parse(readFileSync(EVENTS_FILE, 'utf8')); } catch { return {}; }
}

function writeEvents(events) {
  ensureSoundsDir();
  writeFileSync(EVENTS_FILE, JSON.stringify(events, null, 2), 'utf8');
}

// GET /sounds — list all sound files
router.get('/', (_req, res) => {
  try {
    ensureSoundsDir();
    const files = readdirSync(SOUNDS_DIR)
      .filter(f => ALLOWED_EXTS.has(extname(f).toLowerCase()))
      .map(f => {
        const stat = statSync(join(SOUNDS_DIR, f));
        return { filename: f, size: stat.size, mtime: stat.mtime };
      })
      .sort((a, b) => a.filename.localeCompare(b.filename));
    res.json(files);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET /sounds/events — get event mapping (before /:filename so it matches first)
router.get('/events', (_req, res) => {
  try {
    res.json(readEvents());
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// PATCH /sounds/events/:event — assign a sound to an event
router.patch('/events/:event', requireAuth, requireRole('admin'), (req, res) => {
  try {
    const { event } = req.params;
    const { filename } = req.body;
    if (!event) return res.status(400).json({ error: 'Missing event name' });
    const events = readEvents();
    if (filename === null || filename === '') {
      delete events[event];
    } else {
      if (filename && !ALLOWED_EXTS.has(extname(filename).toLowerCase()))
        return res.status(400).json({ error: 'Unsupported file type' });
      events[event] = filename ?? null;
    }
    writeEvents(events);
    res.json(events);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET /sounds/:filename — stream audio file
router.get('/:filename', (req, res) => {
  try {
    const filename = safeFilename(req.params.filename);
    const filePath = join(SOUNDS_DIR, filename);
    if (!existsSync(filePath)) return res.status(404).json({ error: 'Not found' });
    const ext = extname(filename).toLowerCase();
    const stat = statSync(filePath);
    res.setHeader('Content-Type', MIME[ext] || 'application/octet-stream');
    res.setHeader('Content-Length', stat.size);
    res.setHeader('Accept-Ranges', 'bytes');
    createReadStream(filePath).pipe(res);
  } catch (err) {
    res.status(400).json({ error: err.message });
  }
});

// POST /sounds — upload a sound file
router.post('/', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    ensureSoundsDir();
    // Bun / Express: body is raw buffer when Content-Type is audio/*
    // We use the X-Filename header to get the name
    const filename = req.headers['x-filename'];
    if (!filename) return res.status(400).json({ error: 'Missing X-Filename header' });
    safeFilename(filename);
    const filePath = join(SOUNDS_DIR, filename);

    const chunks = [];
    req.on('data', chunk => chunks.push(chunk));
    req.on('end', () => {
      const buf = Buffer.concat(chunks);
      writeFileSync(filePath, buf);
      const stat = statSync(filePath);
      res.status(201).json({ filename, size: stat.size });
    });
    req.on('error', err => res.status(500).json({ error: err.message }));
  } catch (err) {
    res.status(400).json({ error: err.message });
  }
});

// DELETE /sounds/:filename
router.delete('/:filename', requireAuth, requireRole('admin'), (req, res) => {
  try {
    const filename = safeFilename(req.params.filename);
    const filePath = join(SOUNDS_DIR, filename);
    if (!existsSync(filePath)) return res.status(404).json({ error: 'Not found' });
    unlinkSync(filePath);
    res.json({ ok: true });
  } catch (err) {
    res.status(400).json({ error: err.message });
  }
});

export default router;
