import { promises as fs } from 'fs';

const GITHUB_TOKEN       = process.env.GITHUB_TOKEN;
const GITHUB_BACKUP_REPO = process.env.GITHUB_BACKUP_REPO;
const GH_API             = 'https://api.github.com';
const BACKUP_FILE_PATH   = 'zsilencer.archive.gz'; // single file, git history = version history

function ghHeaders(extra = {}) {
  return {
    Authorization: `token ${GITHUB_TOKEN}`,
    Accept: 'application/vnd.github.v3+json',
    'User-Agent': 'zsilencer-backup',
    ...extra,
  };
}

export function isGitHubConfigured() {
  return !!(GITHUB_TOKEN && GITHUB_BACKUP_REPO);
}

/**
 * Commit the backup file to the repo, overwriting the previous one.
 * Git history provides rollback — no separate releases needed.
 * Returns the commit URL.
 */
export async function uploadToGitHub(filepath, _filename, ts) {
  if (!isGitHubConfigured()) throw new Error('GITHUB_TOKEN or GITHUB_BACKUP_REPO not set');

  const fileBuffer = await fs.readFile(filepath);
  const content    = fileBuffer.toString('base64');

  // Get existing file SHA (required by GitHub API to update an existing file)
  let sha;
  const getRes = await fetch(`${GH_API}/repos/${GITHUB_BACKUP_REPO}/contents/${BACKUP_FILE_PATH}`, {
    headers: ghHeaders(),
  });
  if (getRes.ok) {
    sha = (await getRes.json()).sha;
  }

  // Create or update the single backup file
  const body = { message: `backup: ${ts}`, content };
  if (sha) body.sha = sha;

  const putRes = await fetch(`${GH_API}/repos/${GITHUB_BACKUP_REPO}/contents/${BACKUP_FILE_PATH}`, {
    method: 'PUT',
    headers: ghHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify(body),
  });

  if (!putRes.ok) {
    const text = await putRes.text();
    throw new Error(`GitHub push failed: ${putRes.status} ${text}`);
  }

  const result = await putRes.json();
  return result.commit.html_url;
}
