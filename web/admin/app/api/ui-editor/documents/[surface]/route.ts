import { type NextRequest, NextResponse } from "next/server";
import { normalizeUiSurface, validateUiDocument } from "../../../../../lib/ui-layout";
import { readUiLayoutDocument, writeUiLayoutDocument } from "../../../../../lib/ui-layout-store";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

interface SurfaceRouteContext {
  params: {
    surface: string;
  };
}

export async function GET(_req: NextRequest, { params }: SurfaceRouteContext) {
  try {
    return NextResponse.json({
      ok: true,
      document: await readUiLayoutDocument(params.surface),
    });
  } catch (error) {
    return NextResponse.json(
      {
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      },
      { status: 404 },
    );
  }
}

export async function PUT(req: NextRequest, { params }: SurfaceRouteContext) {
  try {
    const body = await req.json();
    const document = validateUiDocument(body.document);
    const routeSurface = normalizeUiSurface(params.surface);
    if (normalizeUiSurface(document.surface) !== routeSurface) {
      return NextResponse.json(
        {
          ok: false,
          error: `Route surface ${routeSurface} does not match document surface ${document.surface}.`,
        },
        { status: 400 },
      );
    }

    return NextResponse.json({
      ok: true,
      ...(await writeUiLayoutDocument(document)),
    });
  } catch (error) {
    return NextResponse.json(
      {
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      },
      { status: 400 },
    );
  }
}
