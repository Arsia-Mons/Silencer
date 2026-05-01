/**
 * GET /api/sprites — list all non-empty sprite banks
 * Proxies to admin-api /api/sprites (the [...]path route only catches sub-paths).
 */
import { type NextRequest, NextResponse } from 'next/server';

const API = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:24080';

export async function GET(_req: NextRequest) {
  try {
    const res = await fetch(`${API}/api/sprites`);
    const data = await res.json();
    return NextResponse.json(data, { status: res.status });
  } catch (e: unknown) {
    return NextResponse.json({ error: String(e) }, { status: 502 });
  }
}
