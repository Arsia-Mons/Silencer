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

function controlRequest(op: string, args: Record<string, unknown> = {}): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const id = Math.floor(Math.random() * 1_000_000) + 1;
    const socket = connect({ host: CONTROL_HOST, port: CONTROL_PORT });
    let buffer = '';

    socket.once('connect', () => {
      socket.write(JSON.stringify({ id, op, args }) + '\n');
    });
    socket.once('error', reject);
    socket.on('data', chunk => {
      buffer += chunk.toString('utf8');
      const newline = buffer.indexOf('\n');
      if (newline < 0) return;
      const line = buffer.slice(0, newline);
      socket.end();
      const reply = JSON.parse(line) as ControlReply;
      if (reply.ok) {
        resolve(reply.result ?? {});
        return;
      }
      const message = reply.code ? `${reply.code}: ${reply.error ?? ''}` : reply.error ?? 'control request failed';
      reject(new Error(message));
    });
    socket.once('close', hadError => {
      if (hadError) return;
      if (!buffer.includes('\n')) reject(new Error('control socket closed without a reply'));
    });
  });
}

export async function POST(req: NextRequest) {
  let tempDir: string | null = null;
  try {
    const body = await req.json();
    const document = validateUiDocument(body.document);

    const preview = await controlRequest('ui_editor_preview', { document });
    await controlRequest('wait_frames', { n: 2 });
    const inspect = await controlRequest('inspect');

    tempDir = await mkdtemp(join(tmpdir(), 'silencer-ui-preview-'));
    const screenshotPath = join(tempDir, `${randomUUID()}.png`);
    await controlRequest('screenshot', { out: screenshotPath });
    const png = await readFile(screenshotPath);

    return NextResponse.json({
      ok: true,
      preview,
      inspect,
      screenshot: `data:image/png;base64,${png.toString('base64')}`,
    });
  } catch (error) {
    return NextResponse.json({
      ok: false,
      error: error instanceof Error ? error.message : String(error),
    }, { status: 502 });
  } finally {
    if (tempDir) await rm(tempDir, { force: true, recursive: true });
  }
}
