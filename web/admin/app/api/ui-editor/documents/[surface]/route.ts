import { type NextRequest, NextResponse } from "next/server";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const API =
  process.env.ADMIN_API_URL || process.env.NEXT_PUBLIC_API_URL || "http://localhost:24080";

interface SurfaceRouteContext {
  params: {
    surface: string;
  };
}

export async function GET(req: NextRequest, { params }: SurfaceRouteContext) {
  try {
    const upstream = await fetch(
      `${API}/api/ui-editor/documents/${encodeURIComponent(params.surface)}`,
      {
        headers: forwardHeaders(req),
        cache: "no-store",
      },
    );
    return proxyJson(upstream);
  } catch (error) {
    return NextResponse.json(
      {
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      },
      { status: 500 },
    );
  }
}

export async function PUT(req: NextRequest, { params }: SurfaceRouteContext) {
  try {
    const upstream = await fetch(
      `${API}/api/ui-editor/documents/${encodeURIComponent(params.surface)}`,
      {
        method: "PUT",
        headers: forwardHeaders(req),
        body: await req.text(),
      },
    );
    return proxyJson(upstream);
  } catch (error) {
    return NextResponse.json(
      {
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      },
      { status: 500 },
    );
  }
}

function forwardHeaders(req: Request): HeadersInit {
  const headers: Record<string, string> = { "Content-Type": "application/json" };
  const auth = req.headers.get("authorization");
  if (auth) headers.authorization = auth;
  return headers;
}

async function proxyJson(response: Response): Promise<NextResponse> {
  const payload = await response.json();
  return NextResponse.json(payload, { status: response.status });
}
