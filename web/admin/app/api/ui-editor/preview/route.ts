import { randomUUID } from 'node:crypto';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { connect } from 'node:net';
import { type NextRequest, NextResponse } from 'next/server';
import { validateUiDocument } from '../../../../lib/ui-layout';

export const runtime = 'nodejs';

type ControlReply = {
  id: number;
  ok: boolean;
  result?: unknown;
  error?: string;
  code?: string;
};

const CONTROL_HOST = process.env.SILENCER_CONTROL_HOST ?? '127.0.0.1';
const CONTROL_PORT = Number.parseInt(process.env.SILENCER_CONTROL_PORT ?? '5170', 10);
const CONTROL_TIMEOUT_MS = Number.parseInt(process.env.SILENCER_CONTROL_TIMEOUT_MS ?? '5000', 10);

let latestPreviewToken = '';
let previewQueue: Promise<unknown> = Promise.resolve();

class StalePreviewError extends Error {
  constructor() {
    super('stale preview request');
  }
}

function controlRequest(op: string, args: Record<string, unknown> = {}): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const id = Math.floor(Math.random() * 1_000_000) + 1;
    const socket = connect({ host: CONTROL_HOST, port: CONTROL_PORT });
    let buffer = '';
    let settled = false;
    let timeout: ReturnType<typeof setTimeout> | undefined;

    const settle = <T,>(callback: (value: T) => void, value: T) => {
      if (settled) return;
      settled = true;
      if (timeout) clearTimeout(timeout);
      socket.destroy();
      callback(value);
    };
    const fail = (error: Error) => settle(reject, error);
    const succeed = (value: unknown) => settle(resolve, value);
    timeout = setTimeout(() => {
      fail(new Error(`control request timed out: ${op}`));
    }, CONTROL_TIMEOUT_MS);

    socket.once('connect', () => {
      socket.write(JSON.stringify({ id, op, args }) + '\n');
    });
    socket.once('error', fail);
    socket.on('data', chunk => {
      buffer += chunk.toString('utf8');
      const newline = buffer.indexOf('\n');
      if (newline < 0) return;
      const line = buffer.slice(0, newline);
      let reply: ControlReply;
      try {
        reply = JSON.parse(line) as ControlReply;
      } catch (error) {
        fail(error instanceof Error ? error : new Error('invalid control reply json'));
        return;
      }
      if (reply.id !== id) {
        fail(new Error(`control reply id mismatch for ${op}`));
        return;
      }
      if (reply.ok) {
        succeed(reply.result ?? {});
        return;
      }
      const message = reply.code ? `${reply.code}: ${reply.error ?? ''}` : reply.error ?? 'control request failed';
      fail(new Error(message));
    });
    socket.once('close', hadError => {
      if (hadError || settled) return;
      if (!buffer.includes('\n')) fail(new Error('control socket closed without a reply'));
    });
  });
}

function assertFreshPreview(token: string) {
  if (token !== latestPreviewToken) throw new StalePreviewError();
}

async function renderPreview(token: string, document: unknown) {
  let tempDir: string | null = null;
  try {
    assertFreshPreview(token);
    const preview = await controlRequest('ui_editor_preview', { document });
    assertFreshPreview(token);
    await controlRequest('wait_frames', { n: 2 });
    assertFreshPreview(token);
    const inspect = await controlRequest('inspect');
    assertFreshPreview(token);

    tempDir = await mkdtemp(join(tmpdir(), 'silencer-ui-preview-'));
    const screenshotPath = join(tempDir, `${randomUUID()}.png`);
    await controlRequest('screenshot', { out: screenshotPath });
    assertFreshPreview(token);
    const png = await readFile(screenshotPath);

    return {
      preview,
      inspect,
      screenshot: `data:image/png;base64,${png.toString('base64')}`,
    };
  } finally {
    if (tempDir) await rm(tempDir, { force: true, recursive: true });
  }
}

function enqueuePreview(token: string, document: unknown) {
  const run = previewQueue
    .catch(() => undefined)
    .then(() => renderPreview(token, document));
  previewQueue = run.catch(() => undefined);
  return run;
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const document = validateUiDocument(body.document);
    const token = randomUUID();
    latestPreviewToken = token;

    const result = await enqueuePreview(token, document);

    return NextResponse.json({
      ok: true,
      ...result,
    });
  } catch (error) {
    if (error instanceof StalePreviewError) {
      return NextResponse.json({ ok: false, stale: true }, { status: 409 });
    }
    return NextResponse.json({
      ok: false,
      error: error instanceof Error ? error.message : String(error),
    }, { status: 502 });
  }
}
