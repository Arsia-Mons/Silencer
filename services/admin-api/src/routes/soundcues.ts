/**
 * Sound Cue endpoints.
 *
 * Sound Cues are small node-graph JSON files stored in
 * shared/assets/gas/sound-cues/<id>.json. Each file is a self-contained
 * graph (nodes + edges) that the game runtime evaluates at play-time to pick
 * a sound dynamically (random, round-robin, mixer, etc.).
 *
 * GET    /sound-cues           list all cue IDs + metadata
 * GET    /sound-cues/:id       return full cue JSON
 * PUT    /sound-cues/:id       save / create a cue (body = cue JSON)
 * DELETE /sound-cues/:id       delete a cue file
 */

import { existsSync, mkdirSync, readdirSync, readFileSync, unlinkSync, writeFileSync } from "fs";
import { basename, join } from "path";
import { Router } from "express";

import { requireAuth } from "../auth/jwt.js";

interface RouteRequest {
  params: Record<string, string>;
  body: unknown;
}

interface RouteResponse {
  json(value: unknown): void;
  status(code: number): RouteResponse;
}

interface SoundCueNode {
  type?: string;
}

interface SoundCueDocument {
  id?: string;
  nodes?: SoundCueNode[];
  edges?: unknown[];
}

const router = Router();

const ASSETS_DIR = process.env.ASSETS_DIR ?? join(import.meta.dir, "../../../../shared/assets");
const CUES_DIR = join(ASSETS_DIR, "gas", "sound-cues");

function ensureCuesDir(): void {
  if (!existsSync(CUES_DIR)) mkdirSync(CUES_DIR, { recursive: true });
}

function sanitizeId(id: string): string {
  return id.replace(/[^a-zA-Z0-9_-]/g, "_");
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function cuePath(id: string): string {
  return join(CUES_DIR, `${sanitizeId(id)}.json`);
}

function parseSoundCue(raw: string): SoundCueDocument {
  return JSON.parse(raw) as SoundCueDocument;
}

function soundCueBody(value: unknown): SoundCueDocument {
  if (!value || typeof value !== "object" || Array.isArray(value)) return {};
  return value as SoundCueDocument;
}

router.get("/", requireAuth, (_req: unknown, res: RouteResponse) => {
  ensureCuesDir();
  try {
    const files = readdirSync(CUES_DIR).filter((file) => file.endsWith(".json"));
    const cues = files.map((file) => {
      const raw = readFileSync(join(CUES_DIR, file), "utf8");
      try {
        const parsed = parseSoundCue(raw);
        return {
          id: parsed.id ?? basename(file, ".json"),
          nodeCount: Array.isArray(parsed.nodes) ? parsed.nodes.length : 0,
          edgeCount: Array.isArray(parsed.edges) ? parsed.edges.length : 0,
        };
      } catch {
        return { id: basename(file, ".json"), nodeCount: 0, edgeCount: 0 };
      }
    });
    res.json(cues);
  } catch (error) {
    res.status(500).json({ error: errorMessage(error) });
  }
});

router.get("/:id", requireAuth, (req: RouteRequest, res: RouteResponse) => {
  ensureCuesDir();
  const path = cuePath(req.params.id);
  if (!existsSync(path)) return res.status(404).json({ error: "Cue not found" });
  try {
    res.json(parseSoundCue(readFileSync(path, "utf8")));
  } catch (error) {
    res.status(500).json({ error: errorMessage(error) });
  }
});

router.put("/:id", requireAuth, (req: RouteRequest, res: RouteResponse) => {
  ensureCuesDir();
  const id = sanitizeId(req.params.id);
  const path = join(CUES_DIR, `${id}.json`);
  try {
    const body = { ...soundCueBody(req.body), id };
    if (!Array.isArray(body.nodes)) {
      return res.status(400).json({ error: "nodes must be an array" });
    }
    if (!Array.isArray(body.edges)) {
      return res.status(400).json({ error: "edges must be an array" });
    }
    const outputNodes = body.nodes.filter((node) => node.type === "Output");
    if (outputNodes.length !== 1) {
      return res.status(400).json({ error: "Cue must have exactly one Output node" });
    }
    writeFileSync(path, JSON.stringify(body, null, 2));
    res.json({ ok: true, id });
  } catch (error) {
    res.status(400).json({ error: errorMessage(error) });
  }
});

router.delete("/:id", requireAuth, (req: RouteRequest, res: RouteResponse) => {
  ensureCuesDir();
  const path = cuePath(req.params.id);
  if (!existsSync(path)) return res.status(404).json({ error: "Cue not found" });
  try {
    unlinkSync(path);
    res.json({ ok: true });
  } catch (error) {
    res.status(500).json({ error: errorMessage(error) });
  }
});

export default router;
