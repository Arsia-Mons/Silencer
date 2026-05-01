'use client';
import { useEffect, useRef } from 'react';

interface Star {
  x: number; y: number;
  size: number;
  base: number;
  twinkleAmp: number;
  twinkleSpeed: number;
  phase: number;
}

interface Comet {
  x: number; y: number;
  vx: number; vy: number;
  tailLen: number;
  width: number;
  alpha: number;
}

const STAR_COUNT = 350;
const MAX_COMETS = 12;
const COMET_SPAWN_CHANCE = 0.02;

function buildStars(w: number, h: number): Star[] {
  return Array.from({ length: STAR_COUNT }, () => {
    const isBright = Math.random() < 0.35;
    return {
      x: Math.random() * w,
      y: Math.random() * h,
      size: isBright ? Math.random() * 1.4 + 0.7 : Math.random() * 0.7 + 0.2,
      base: isBright ? Math.random() * 0.3 + 0.5 : Math.random() * 0.3 + 0.1,
      twinkleAmp: isBright ? Math.random() * 0.5 + 0.2 : Math.random() < 0.4 ? Math.random() * 0.2 + 0.05 : 0,
      twinkleSpeed: Math.random() * 1.5 + 0.3,
      phase: Math.random() * Math.PI * 2,
    };
  });
}

function spawnComet(w: number, h: number): Comet {
  const angle = (Math.random() * (5 / 6) + 1 / 12) * Math.PI;
  const speed = Math.random() * 5 + 3;
  const vx = Math.cos(angle) * speed;
  const vy = Math.sin(angle) * speed;
  const fromTop = Math.random() < 0.5;
  return {
    x: fromTop ? Math.random() * w : -80,
    y: fromTop ? -80 : Math.random() * h * 0.6,
    vx, vy,
    tailLen: Math.random() * 140 + 80,
    width: Math.random() * 1.2 + 0.6,
    alpha: Math.random() * 0.4 + 0.6,
  };
}

function drawComet(ctx: CanvasRenderingContext2D, c: Comet) {
  const speed = Math.sqrt(c.vx * c.vx + c.vy * c.vy);
  const nx = (-c.vx / speed) * c.tailLen;
  const ny = (-c.vy / speed) * c.tailLen;

  const grad = ctx.createLinearGradient(c.x, c.y, c.x + nx, c.y + ny);
  grad.addColorStop(0,    `rgba(255,255,245,${c.alpha.toFixed(3)})`);
  grad.addColorStop(0.15, `rgba(200,220,255,${(c.alpha * 0.7).toFixed(3)})`);
  grad.addColorStop(0.5,  `rgba(160,190,255,${(c.alpha * 0.3).toFixed(3)})`);
  grad.addColorStop(1,    `rgba(120,160,255,0)`);

  ctx.beginPath();
  ctx.moveTo(c.x, c.y);
  ctx.lineTo(c.x + nx, c.y + ny);
  ctx.strokeStyle = grad;
  ctx.lineWidth = c.width;
  ctx.lineCap = 'round';
  ctx.stroke();

  ctx.beginPath();
  ctx.arc(c.x, c.y, c.width * 1.8, 0, Math.PI * 2);
  ctx.fillStyle = `rgba(255,255,255,${c.alpha.toFixed(3)})`;
  ctx.fill();
}

export default function Starfield() {
  const starCanvasRef  = useRef<HTMLCanvasElement>(null);
  const cometCanvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const starCanvas  = starCanvasRef.current!;
    const cometCanvas = cometCanvasRef.current!;
    const sCtx = starCanvas.getContext('2d')!;
    const cCtx = cometCanvas.getContext('2d')!;
    let raf: number;
    let stars: Star[] = [];
    let comets: Comet[] = [];

    // Parallax: target and smoothed offset
    let targetX = 0, targetY = 0;
    let smoothX = 0, smoothY = 0;
    const PARALLAX_STRENGTH = 25; // max px offset
    const PARALLAX_LERP = 0.05;   // smoothing (lower = lazier)

    const onMouseMove = (e: MouseEvent) => {
      const w = starCanvas.width, h = starCanvas.height;
      targetX = ((e.clientX / w) - 0.5) * PARALLAX_STRENGTH;
      targetY = ((e.clientY / h) - 0.5) * PARALLAX_STRENGTH;
    };
    window.addEventListener('mousemove', onMouseMove);

    const resize = () => {
      starCanvas.width  = cometCanvas.width  = window.innerWidth;
      starCanvas.height = cometCanvas.height = window.innerHeight;
      stars = buildStars(starCanvas.width, starCanvas.height);
    };
    resize();
    window.addEventListener('resize', resize);

    // Seed with comets immediately so they're visible on load
    for (let i = 0; i < 4; i++)
      comets.push(spawnComet(cometCanvas.width, cometCanvas.height));

    const draw = (t: number) => {
      const w = starCanvas.width;
      const h = starCanvas.height;
      const ts = t / 1000;

      // Smooth parallax
      smoothX += (targetX - smoothX) * PARALLAX_LERP;
      smoothY += (targetY - smoothY) * PARALLAX_LERP;

      // Stars — behind the dark overlay, offset by parallax
      sCtx.clearRect(0, 0, w, h);
      for (const s of stars) {
        const alpha = s.twinkleAmp > 0
          ? s.base + s.twinkleAmp * Math.sin(ts * s.twinkleSpeed + s.phase)
          : s.base;
        sCtx.beginPath();
        sCtx.arc(s.x + smoothX, s.y + smoothY, s.size, 0, Math.PI * 2);
        sCtx.fillStyle = `rgba(220,230,255,${alpha.toFixed(3)})`;
        sCtx.fill();
      }

      // Comets — above the dark overlay
      cCtx.clearRect(0, 0, w, h);
      if (comets.length < MAX_COMETS && Math.random() < COMET_SPAWN_CHANCE) {
        comets.push(spawnComet(w, h));
      }
      comets = comets.filter(c => {
        c.x += c.vx;
        c.y += c.vy;
        const margin = 120;
        if (c.x > w - margin) c.alpha *= 0.96;
        if (c.y > h - margin) c.alpha *= 0.96;
        drawComet(cCtx, c);
        return c.x < w + 100 && c.y < h + 100 && c.alpha > 0.02;
      });

      raf = requestAnimationFrame(draw);
    };

    raf = requestAnimationFrame(draw);
    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener('resize', resize);
      window.removeEventListener('mousemove', onMouseMove);
    };
  }, []);

  return (
    <>
      {/* Stars sit behind the dark overlay (z-index 1) */}
      <canvas ref={starCanvasRef}  style={{ position: 'fixed', inset: 0, zIndex: 0, pointerEvents: 'none' }} />
      {/* Comets sit above the dark overlay but below page content (z-index 2) */}
      <canvas ref={cometCanvasRef} style={{ position: 'fixed', inset: 0, zIndex: 2, pointerEvents: 'none' }} />
    </>
  );
}
