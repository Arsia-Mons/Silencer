import { useEffect, useRef, useState } from "react";
import { type UiDocument } from "../../lib/ui-layout";

export type ClientPreviewElement = {
  id?: string;
  label?: string;
  kind?: string;
  source: "element" | "widget";
  x: number;
  y: number;
  w: number;
  h: number;
};

export type ClientPreviewState = {
  status: "idle" | "syncing" | "live" | "offline";
  screenshot?: string;
  elements: ClientPreviewElement[];
  error?: string;
};

export function useClientPreview(document: UiDocument, hydrated: boolean): ClientPreviewState {
  const [clientPreview, setClientPreview] = useState<ClientPreviewState>({
    status: "idle",
    elements: [],
  });
  const requestSeq = useRef(0);
  const sessionId = useRef<string>(
    typeof crypto !== "undefined" && typeof crypto.randomUUID === "function"
      ? crypto.randomUUID()
      : `${Date.now()}-${Math.random()}`,
  );

  useEffect(() => {
    if (!hydrated) return;
    const seq = requestSeq.current + 1;
    requestSeq.current = seq;
    const controller = new AbortController();
    const timer = window.setTimeout(async () => {
      setClientPreview((prev) => ({ ...prev, status: "syncing", error: undefined }));
      try {
        const response = await fetch("/ui-editor/preview", {
          method: "POST",
          headers: previewHeaders(),
          body: JSON.stringify({ document, sessionId: sessionId.current, generation: seq }),
          signal: controller.signal,
        });
        const data = await response.json();
        if (controller.signal.aborted || seq !== requestSeq.current) return;
        if (!response.ok || !data.ok) {
          if (data.stale) return;
          throw new Error(data.error ?? "CLIENT PREVIEW FAILED");
        }
        setClientPreview({
          status: "live",
          screenshot: data.screenshot,
          elements: inspectElements(data.inspect),
        });
      } catch (error) {
        if (controller.signal.aborted || seq !== requestSeq.current) return;
        setClientPreview((prev) => ({
          ...prev,
          status: "offline",
          error: error instanceof Error ? error.message : "CLIENT PREVIEW FAILED",
        }));
      }
    }, 240);
    return () => {
      controller.abort();
      window.clearTimeout(timer);
    };
  }, [document, hydrated]);

  return clientPreview;
}

function previewHeaders(): HeadersInit {
  const token = typeof window === "undefined" ? null : localStorage.getItem("zs_token");
  return {
    "Content-Type": "application/json",
    ...(token ? { Authorization: `Bearer ${token}` } : {}),
  };
}

function inspectElements(inspect: unknown): ClientPreviewElement[] {
  if (!inspect || typeof inspect !== "object") return [];
  const source = inspect as { elements?: unknown; widgets?: unknown };
  const elements = new Map<string, ClientPreviewElement>();
  const anonymous: ClientPreviewElement[] = [];

  readPreviewItems(source.elements, "element", elements, anonymous);
  readPreviewItems(source.widgets, "widget", elements, anonymous);

  return [...elements.values(), ...anonymous];
}

function readPreviewItems(
  value: unknown,
  source: "element" | "widget",
  elements: Map<string, ClientPreviewElement>,
  anonymous: ClientPreviewElement[],
) {
  if (!Array.isArray(value)) return;
  for (const item of value) {
    if (!item || typeof item !== "object") continue;
    const candidate = item as Record<string, unknown>;
    const x = numeric(candidate.x);
    const y = numeric(candidate.y);
    const w = numeric(candidate.w);
    const h = numeric(candidate.h);
    if (x === null || y === null || w === null || h === null || w <= 0 || h <= 0) continue;
    const element: ClientPreviewElement = {
      id: typeof candidate.id === "string" ? candidate.id : undefined,
      label: typeof candidate.label === "string" ? candidate.label : undefined,
      kind: typeof candidate.kind === "string" ? candidate.kind : undefined,
      source,
      x,
      y,
      w,
      h,
    };
    if (!element.id) {
      anonymous.push(element);
      continue;
    }
    if (source === "element" || !elements.has(element.id)) {
      elements.set(element.id, element);
    }
  }
}

function numeric(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}
