'use client';
import { useEffect, useRef } from 'react';

interface Star {
  x: number; y: number;
  size: number;
  base: number;        // base alpha
  twinkleAmp: number;  // 0 = static, >0 = twinkling
  twinkleSpeed: number;
  phase: number;
}

const STAR_COUNT = 350;

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

export default function Starfield() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current!;
    const ctx = canvas.getContext('2d')!;
    let raf: number;
    let stars: Star[] = [];

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
      for (const s of stars) {
        const alpha = s.twinkleAmp > 0
          ? s.base + s.twinkleAmp * Math.sin(ts * s.twinkleSpeed + s.phase)
          : s.base;
        ctx.beginPath();
        ctx.arc(s.x, s.y, s.size, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(220,230,255,${alpha.toFixed(3)})`;
        ctx.fill();
      }
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
