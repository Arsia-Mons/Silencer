/**
 * Next.js API proxy: /api/actors (root) → admin-api /api/actors
 * Handles GET list and POST (unused, but forwarded for completeness).
 */
import { type NextRequest, NextResponse } from 'next/server';

const API = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080';

function fwdHeaders(req: NextRequest): Record<string, string> {
  const h: Record<string, string> = { 'Content-Type': 'application/json' };
  const auth = req.headers.get('authorization');
  if (auth) h['authorization'] = auth;
  return h;
}

export async function GET(req: NextRequest) {
  const res = await fetch(`${API}/api/actors`, { headers: fwdHeaders(req) });
  const data = await res.json();
  return NextResponse.json(data, { status: res.status });
}
