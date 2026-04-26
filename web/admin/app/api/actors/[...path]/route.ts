/**
 * Next.js API proxy: /api/actors/[...path] → admin-api /actors/[...path]
 */
import { type NextRequest, NextResponse } from 'next/server';

const API = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080';

function fwdHeaders(req: NextRequest): Record<string, string> {
  const h: Record<string, string> = { 'Content-Type': 'application/json' };
  const auth = req.headers.get('authorization');
  if (auth) h['authorization'] = auth;
  return h;
}

async function proxy(req: NextRequest, path: string, method: string, body?: BodyInit) {
  const upstream = `${API}/actors/${path}`;
  const res = await fetch(upstream, { method, headers: fwdHeaders(req), body });
  const data = await res.json();
  return NextResponse.json(data, { status: res.status });
}

export async function GET(req: NextRequest, { params }: { params: { path: string[] } }) {
  return proxy(req, params.path.join('/'), 'GET');
}

export async function PUT(req: NextRequest, { params }: { params: { path: string[] } }) {
  const body = await req.text();
  return proxy(req, params.path.join('/'), 'PUT', body);
}

export async function DELETE(req: NextRequest, { params }: { params: { path: string[] } }) {
  return proxy(req, params.path.join('/'), 'DELETE');
}
