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
let previewQueue: Promise<unknown> = Promise.resolve();
const latestPreviewGenerationBySession = new Map<string, number>();
const activePreviewBySession = new Map<string, AbortController>();

class StalePreviewError extends Error {
  constructor() {
    super('stale preview request');
  }
}

function controlRequest(op: string, args: Record<string, unknown> = {}, signal?: AbortSignal): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const id = Math.floor(Math.random() * 1_000_000) + 1;
    const socket = connect({ host: CONTROL_HOST, port: CONTROL_PORT });
    let buffer = '';
    let settled = false;
    let timeout: ReturnType<typeof setTimeout> | undefined;
    const abort = () => fail(new StalePreviewError());

    const settle = <T,>(callback: (value: T) => void, value: T) => {
      if (settled) return;
      settled = true;
      if (timeout) clearTimeout(timeout);
      signal?.removeEventListener('abort', abort);
      socket.destroy();
      callback(value);
    };
    const fail = (error: Error) => settle(reject, error);
    const succeed = (value: unknown) => settle(resolve, value);
    if (signal?.aborted) {
      fail(new StalePreviewError());
      return;
    }
    signal?.addEventListener('abort', abort, { once: true });
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

function assertLatestPreview(sessionId: string, generation: number) {
  if (latestPreviewGenerationBySession.get(sessionId) !== generation) {
    throw new StalePreviewError();
  }
}

async function renderPreview(sessionId: string, generation: number, document: unknown, signal: AbortSignal) {
  let tempDir: string | null = null;
  try {
    if (signal.aborted) throw new StalePreviewError();
    assertLatestPreview(sessionId, generation);
    tempDir = await mkdtemp(join(tmpdir(), 'silencer-ui-preview-'));
    const screenshotPath = join(tempDir, `${randomUUID()}.png`);
    if (signal.aborted) throw new StalePreviewError();
    assertLatestPreview(sessionId, generation);
    const capture = await controlRequest('ui_editor_preview_capture', {
      document,
      out: screenshotPath,
    }, signal) as { preview?: unknown; inspect?: unknown };
    if (signal.aborted) throw new StalePreviewError();
    assertLatestPreview(sessionId, generation);
    const png = await readFile(screenshotPath);

    return {
      preview: capture.preview ?? {},
      inspect: capture.inspect ?? {},
      screenshot: `data:image/png;base64,${png.toString('base64')}`,
    };
  } finally {
    if (tempDir) await rm(tempDir, { force: true, recursive: true });
  }
}

function enqueuePreview(sessionId: string, generation: number, document: unknown, signal: AbortSignal) {
  const run = previewQueue
    .catch(() => undefined)
    .then(() => renderPreview(sessionId, generation, document, signal));
  previewQueue = run.catch(() => undefined);
  return run;
}

function makePreviewSignal(sessionId: string, requestSignal: AbortSignal): { signal: AbortSignal; dispose: () => void } {
  const controller = new AbortController();
  const abort = () => controller.abort();
  requestSignal.addEventListener('abort', abort, { once: true });
  activePreviewBySession.get(sessionId)?.abort();
  activePreviewBySession.set(sessionId, controller);
  const dispose = () => {
    requestSignal.removeEventListener('abort', abort);
    if (activePreviewBySession.get(sessionId) === controller) {
      activePreviewBySession.delete(sessionId);
    }
  };
  controller.signal.addEventListener('abort', dispose, { once: true });
  return { signal: controller.signal, dispose };
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const document = validateUiDocument(body.document);
    const sessionId = typeof body.sessionId === 'string' && body.sessionId.length > 0
      ? body.sessionId
      : 'default';
    const latestForSession = latestPreviewGenerationBySession.get(sessionId) ?? 0;
    const generation = typeof body.generation === 'number' && Number.isInteger(body.generation)
      ? body.generation
      : 0;
    if (generation < latestForSession) throw new StalePreviewError();
    const previewGeneration = generation > 0 ? generation : latestForSession + 1;
    latestPreviewGenerationBySession.set(sessionId, previewGeneration);
    if (req.signal.aborted) throw new StalePreviewError();
    const previewSignal = makePreviewSignal(sessionId, req.signal);

    let result: Awaited<ReturnType<typeof enqueuePreview>>;
    try {
      result = await enqueuePreview(sessionId, previewGeneration, document, previewSignal.signal);
    } finally {
      previewSignal.dispose();
    }

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
