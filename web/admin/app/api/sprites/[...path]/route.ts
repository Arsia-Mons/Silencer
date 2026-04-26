/**
 * Next.js API proxy: /api/sprites/[...path] → admin-api /api/sprites/[...path]
 * Forwards the Authorization header so <img> tags can load sprite PNGs.
 */
import { type NextRequest, NextResponse } from 'next/server';

const API = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080';

export async function GET(
  req: NextRequest,
  { params }: { params: { path: string[] } }
) {
  const path = params.path.join('/');
  const url = new URL(req.url);
  const upstream = `${API}/api/sprites/${path}${url.search}`;

  const headers: Record<string, string> = {};
  const auth = req.headers.get('authorization');
  if (auth) headers['authorization'] = auth;

  const res = await fetch(upstream, { headers });
  const body = await res.arrayBuffer();

  return new NextResponse(body, {
    status: res.status,
    headers: {
      'Content-Type': res.headers.get('Content-Type') || 'application/octet-stream',
      'Cache-Control': res.headers.get('Cache-Control') || 'no-cache',
    },
  });
}
