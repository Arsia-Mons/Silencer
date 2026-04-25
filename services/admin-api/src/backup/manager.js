import { execFile } from 'child_process';
import { promises as fs } from 'fs';
import path from 'path';
import { uploadToGitHub, isGitHubConfigured } from './github.js';

const BACKUP_DIR     = process.env.BACKUP_DIR || '/backups';
const MONGO_URI      = process.env.MONGO_URL   || 'mongodb://mongo:27017/silencer';
const MAX_BACKUPS    = parseInt(process.env.BACKUP_KEEP || '10', 10);
const MONGODUMP_BIN  = process.env.MONGODUMP_BIN || 'mongodump';

let state = {
  inProgress: false,
  lastResult: null, // { ok, filename, sizeKB, ts, githubUrl?, error? }
  githubConfigured: isGitHubConfigured(),
};

export function getBackupState() { return { ...state }; }

async function ensureDir() {
  await fs.mkdir(BACKUP_DIR, { recursive: true });
}

async function pruneOldBackups() {
  const files = await listBackupFiles();
  const excess = files.slice(MAX_BACKUPS);
  for (const f of excess) {
    await fs.unlink(path.join(BACKUP_DIR, f.filename)).catch(() => {});
  }
}

export async function listBackupFiles() {
  await ensureDir();
  const entries = await fs.readdir(BACKUP_DIR);
  const results = [];
  for (const name of entries.filter(n => n.endsWith('.gz'))) {
    try {
      const stat = await fs.stat(path.join(BACKUP_DIR, name));
      results.push({ filename: name, sizeKB: Math.round(stat.size / 1024), ts: stat.mtime.toISOString() });
    } catch { /* skip */ }
  }
  return results.sort((a, b) => (b.ts > a.ts ? 1 : -1));
}

export function triggerBackup() {
  if (state.inProgress) return { ok: false, inProgress: true };
  state.inProgress = true;

  const ts       = new Date().toISOString().replace(/[:.]/g, '-');
  const filename = `zsilencer-${ts}.archive.gz`;
  const outPath  = path.join(BACKUP_DIR, filename);

  ensureDir().then(() =>
    new Promise((resolve, reject) => {
      execFile(
        MONGODUMP_BIN,
        ['--uri', MONGO_URI, `--archive=${outPath}`, '--gzip'],
        { timeout: 120_000 },
        (err, _stdout, stderr) => {
          if (err) reject(new Error(stderr || err.message));
          else resolve();
        },
      );
    })
  ).then(async () => {
    const stat = await fs.stat(outPath);
    const result = { ok: true, filename, sizeKB: Math.round(stat.size / 1024), ts: new Date().toISOString() };

    // Upload to GitHub Releases if configured
    if (isGitHubConfigured()) {
      try {
        result.githubUrl = await uploadToGitHub(outPath, filename, ts);
        console.log(`[backup] uploaded to GitHub: ${result.githubUrl}`);
      } catch (e) {
        result.githubError = e.message;
        console.error(`[backup] GitHub upload failed: ${e.message}`);
      }
    }

    state.lastResult = result;
    await pruneOldBackups();
  }).catch(async (err) => {
    state.lastResult = { ok: false, error: err.message, ts: new Date().toISOString() };
    // remove partial file if present
    await fs.unlink(outPath).catch(() => {});
  }).finally(() => {
    state.inProgress = false;
  });

  return { ok: true, inProgress: true, filename };
}
