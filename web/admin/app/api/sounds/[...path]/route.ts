/**
 * Next.js API proxy: /api/sounds/[...path] → admin-api /api/sounds/[...path]
 *
 * Uses streaming passthrough so binary audio responses (play endpoint)
 * are forwarded without buffering into JSON.
 */
import { type NextRequest, NextResponse } from 'next/server';

const API = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080';

function fwdHeaders(req: NextRequest): Record<string, string> {
  const h: Record<string, string> = {};
  const auth = req.headers.get('authorization');
  if (auth) h['authorization'] = auth;
  const ct = req.headers.get('content-type');
  if (ct) h['content-type'] = ct;
  const xf = req.headers.get('x-filename');
  if (xf) h['x-filename'] = xf;
  return h;
}

async function proxy(req: NextRequest, path: string) {
  const url = `${API}/api/sounds/${path}`;
  const body = req.method !== 'GET' && req.method !== 'HEAD' ? req.body : undefined;
  const upstream = await fetch(url, {
    method: req.method,
    headers: fwdHeaders(req),
    body,
    // @ts-expect-error Node fetch duplex
    duplex: 'half',
  });

  // Stream the response body as-is (handles binary audio)
  return new NextResponse(upstream.body, {
    status: upstream.status,
    headers: {
      'content-type': upstream.headers.get('content-type') || 'application/octet-stream',
      ...(upstream.headers.get('content-length')
        ? { 'content-length': upstream.headers.get('content-length')! }
        : {}),
    },
  });
}

export async function GET(req: NextRequest, { params }: { params: Promise<{ path: string[] }> }) {
  const { path } = await params;
  return proxy(req, path.join('/'));
}

export async function POST(req: NextRequest, { params }: { params: Promise<{ path: string[] }> }) {
  const { path } = await params;
  return proxy(req, path.join('/'));
}

export async function DELETE(req: NextRequest, { params }: { params: Promise<{ path: string[] }> }) {
  const { path } = await params;
  return proxy(req, path.join('/'));
}

export async function PATCH(req: NextRequest, { params }: { params: Promise<{ path: string[] }> }) {
  const { path } = await params;
  return proxy(req, path.join('/'));
}
