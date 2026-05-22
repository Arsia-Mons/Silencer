/**
 * Catch-all proxy: any /api/** route not handled by a more-specific Next.js
 * route handler is forwarded server-side to the admin-api.
 *
 * This makes relative API calls work when the Next.js server and the admin-api
 * are on different origins (local / LAN access without a Cloudflare Tunnel).
 *
 * Precedence: Next.js resolves more-specific segments first, so
 *   /api/sounds/[...path]  beats  /api/[...path]  for /api/sounds/* requests.
 */
import { type NextRequest, NextResponse } from 'next/server';

const UPSTREAM = (process.env.ADMIN_API_URL || process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080').replace(/\/$/, '');

async function proxy(req: NextRequest, { params }: { params: { path: string[] } }) {
  const subpath = params.path.join('/');
  const search = req.nextUrl.search ?? '';
  const url = `${UPSTREAM}/api/${subpath}${search}`;

  const headers: Record<string, string> = {};
  const auth = req.headers.get('authorization');
  if (auth) headers['authorization'] = auth;
  const ct = req.headers.get('content-type');
  if (ct) headers['content-type'] = ct;

  const body = req.method !== 'GET' && req.method !== 'HEAD' ? req.body : undefined;

  try {
    const upstream = await fetch(url, {
      method: req.method,
      headers,
      body,
      // @ts-expect-error Node fetch duplex
      duplex: 'half',
    });

    const contentType = upstream.headers.get('content-type') ?? '';
    if (contentType.includes('application/json')) {
      const data = await upstream.json();
      return NextResponse.json(data, { status: upstream.status });
    }
    const buf = await upstream.arrayBuffer();
    return new NextResponse(buf, {
      status: upstream.status,
      headers: { 'content-type': contentType },
    });
  } catch (e: any) {
    return NextResponse.json({ error: e.message }, { status: 502 });
  }
}

export const GET    = proxy;
export const POST   = proxy;
export const PUT    = proxy;
export const PATCH  = proxy;
export const DELETE = proxy;
