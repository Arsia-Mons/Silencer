/**
 * Next.js proxy: /api/sound-cues/[id] → admin-api /api/sound-cues/:id
 */
import { type NextRequest, NextResponse } from 'next/server';

const API = process.env.ADMIN_API_URL || process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080';

function fwdHeaders(req: NextRequest): Record<string, string> {
  const h: Record<string, string> = {};
  const auth = req.headers.get('authorization');
  if (auth) h['authorization'] = auth;
  const ct = req.headers.get('content-type');
  if (ct) h['content-type'] = ct;
  return h;
}

type Ctx = { params: Promise<{ id: string }> };

async function proxy(req: NextRequest, id: string) {
  const body = req.method !== 'GET' ? req.body : undefined;
  const upstream = await fetch(`${API}/api/sound-cues/${id}`, {
    method: req.method,
    headers: fwdHeaders(req),
    body,
    // @ts-expect-error Node fetch duplex
    duplex: 'half',
  });
  const data = await upstream.json();
  return NextResponse.json(data, { status: upstream.status });
}

export async function GET(req: NextRequest, { params }: Ctx) {
  const { id } = await params; return proxy(req, id);
}
export async function PUT(req: NextRequest, { params }: Ctx) {
  const { id } = await params; return proxy(req, id);
}
export async function DELETE(req: NextRequest, { params }: Ctx) {
  const { id } = await params; return proxy(req, id);
}
