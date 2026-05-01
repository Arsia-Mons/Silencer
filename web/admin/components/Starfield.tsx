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
const MAX_COMETS = 4;
const COMET_SPAWN_CHANCE = 0.0015; // per frame

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
  // Random angle roughly diagonal across screen (30–150°)
  const angle = (Math.random() * (5 / 6) + 1 / 12) * Math.PI;
  const speed = Math.random() * 5 + 3;
  const vx = Math.cos(angle) * speed;
  const vy = Math.sin(angle) * speed;
  // Spawn off-screen on the top or left edge
  const fromTop = Math.random() < 0.5;
  return {
    x: fromTop ? Math.random() * w : -80,
    y: fromTop ? -80 : Math.random() * h * 0.6,
    vx, vy,
    tailLen: Math.random() * 140 + 80,
    width: Math.random() * 1.2 + 0.6,
    alpha: Math.random() * 0.5 + 0.5,
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

  // Bright head
  ctx.beginPath();
  ctx.arc(c.x, c.y, c.width * 1.8, 0, Math.PI * 2);
  ctx.fillStyle = `rgba(255,255,255,${c.alpha.toFixed(3)})`;
  ctx.fill();
}

export default function Starfield() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current!;
    const ctx = canvas.getContext('2d')!;
    let raf: number;
    let stars: Star[] = [];
    let comets: Comet[] = [];

    const resize = () => {
      canvas.width  = window.innerWidth;
      canvas.height = window.innerHeight;
      stars = buildStars(canvas.width, canvas.height);
    };
    resize();
    window.addEventListener('resize', resize);

    const draw = (t: number) => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      const ts = t / 1000;
      const w = canvas.width;
      const h = canvas.height;

      // Stars
      for (const s of stars) {
        const alpha = s.twinkleAmp > 0
          ? s.base + s.twinkleAmp * Math.sin(ts * s.twinkleSpeed + s.phase)
          : s.base;
        ctx.beginPath();
        ctx.arc(s.x, s.y, s.size, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(220,230,255,${alpha.toFixed(3)})`;
        ctx.fill();
      }

      // Spawn comets
      if (comets.length < MAX_COMETS && Math.random() < COMET_SPAWN_CHANCE) {
        comets.push(spawnComet(w, h));
      }

      // Update + draw comets
      comets = comets.filter(c => {
        c.x += c.vx;
        c.y += c.vy;
        // Fade out near edges
        const margin = 120;
        if (c.x > w - margin) c.alpha *= 0.96;
        if (c.y > h - margin) c.alpha *= 0.96;
        drawComet(ctx, c);
        return c.x < w + 100 && c.y < h + 100 && c.alpha > 0.02;
      });

      raf = requestAnimationFrame(draw);
    };

    raf = requestAnimationFrame(draw);
    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener('resize', resize);
    };
  }, []);

  return (
    <canvas
      ref={canvasRef}
      style={{ position: 'fixed', inset: 0, zIndex: 0, pointerEvents: 'none' }}
    />
  );
}
