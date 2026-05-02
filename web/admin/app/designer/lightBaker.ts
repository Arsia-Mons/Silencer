import type { MapActor, MapPlatform, MapShadowZone } from '../../lib/types';

export interface BakedLightMask {
  x: number;
  y: number;
  diam: number;
  data: Uint8Array; // diam×diam, 0=wall-occluded or outside circle, 1=lit
}

export const LIGHT_RADII = [80, 140, 200] as const;

type AABB = { x1: number; y1: number; x2: number; y2: number };

export function buildOccluders(platforms: MapPlatform[], shadowZones: MapShadowZone[]): AABB[] {
  const out: AABB[] = [];
  for (const p of platforms) {
    if (!(p.type1 & 1)) continue; // RECTANGLE bit only
    out.push({
      x1: Math.min(p.x1, p.x2), y1: Math.min(p.y1, p.y2),
      x2: Math.max(p.x1, p.x2), y2: Math.max(p.y1, p.y2),
    });
  }
  for (const z of shadowZones) {
    out.push({
      x1: Math.min(z.x1, z.x2), y1: Math.min(z.y1, z.y2),
      x2: Math.max(z.x1, z.x2), y2: Math.max(z.y1, z.y2),
    });
  }
  return out;
}

function isOccluded(lx: number, ly: number, px: number, py: number, occluders: AABB[]): boolean {
  const rdx = px - lx;
  const rdy = py - ly;
  for (const o of occluders) {
    if (lx >= o.x1 && lx <= o.x2 && ly >= o.y1 && ly <= o.y2) continue; // light inside occluder
    let tmin = 0.01, tmax = 0.99;
    if (rdx === 0) {
      if (lx < o.x1 || lx > o.x2) continue;
    } else {
      let tx1 = (o.x1 - lx) / rdx, tx2 = (o.x2 - lx) / rdx;
      if (tx1 > tx2) { const t = tx1; tx1 = tx2; tx2 = t; }
      if (tx1 > tmin) tmin = tx1;
      if (tx2 < tmax) tmax = tx2;
      if (tmin > tmax) continue;
    }
    if (rdy === 0) {
      if (ly < o.y1 || ly > o.y2) continue;
    } else {
      let ty1 = (o.y1 - ly) / rdy, ty2 = (o.y2 - ly) / rdy;
      if (ty1 > ty2) { const t = ty1; ty1 = ty2; ty2 = t; }
      if (ty1 > tmin) tmin = ty1;
      if (ty2 < tmax) tmax = ty2;
      if (tmin > tmax) continue;
    }
    if (tmin < 1 && tmax > 0) return true;
  }
  return false;
}

export function bakeSingleLight(
  lx: number, ly: number, radius: number,
  occluders: AABB[],
  shape: number,
  direction: number,
): Uint8Array {
  const diam = radius * 2;
  const data = new Uint8Array(diam * diam);
  const r2 = radius * radius;
  // Cull occluders to those overlapping the light bounding box
  const localOcc = occluders.filter(o =>
    o.x2 >= lx - radius && o.x1 <= lx + radius &&
    o.y2 >= ly - radius && o.y1 <= ly + radius
  );

  const HALF_ANGLE = Math.PI / 4; // 45°
  const cosHalf = Math.cos(HALF_ANGLE);
  const DIR_ANGLES = [0, -Math.PI/4, -Math.PI/2, -3*Math.PI/4, Math.PI, 3*Math.PI/4, Math.PI/2, Math.PI/4];
  const dirAngle = DIR_ANGLES[direction & 7];
  const cosDirX = Math.cos(dirAngle);
  const sinDirY = Math.sin(dirAngle);

  for (let my = 0; my < diam; my++) {
    for (let mx = 0; mx < diam; mx++) {
      const dx = mx - radius;
      const dy = my - radius;
      if (dx * dx + dy * dy >= r2) continue; // outside circle
      if (shape === 1) {
        // Spot light: clip to cone before occlusion
        const dist = Math.hypot(dx, dy);
        if (dist > 0.5) {
          const dot = (dx/dist) * cosDirX + (dy/dist) * sinDirY;
          if (dot < cosHalf) continue; // outside cone
        }
      }
      const px = lx + dx;
      const py = ly + dy;
      if (!isOccluded(lx, ly, px, py, localOcc)) {
        data[my * diam + mx] = 1;
      }
    }
  }
  return data;
}

export function bakeMapLightMasks(
  actors: MapActor[],
  platforms: MapPlatform[],
  shadowZones: MapShadowZone[],
): BakedLightMask[] {
  const occluders = buildOccluders(platforms, shadowZones);
  const masks: BakedLightMask[] = [];
  for (const actor of actors) {
    if (actor.id !== 71) continue; // only light overlays (type 71)
    const size = (actor.type >>> 0) & 3;
    const shape = (actor.type >>> 2) & 1;
    const radius = LIGHT_RADII[size] ?? 80;
    const data = bakeSingleLight(actor.x, actor.y, radius, occluders, shape, actor.direction & 7);
    masks.push({ x: actor.x, y: actor.y, diam: radius * 2, data });
  }
  return masks;
}
