'use client';
import { useRouter } from 'next/navigation';
import { useEffect } from 'react';

function isExpired(token: string): boolean {
  try {
    const payload = JSON.parse(atob(token.split('.')[1]));
    return (payload.exp as number) * 1000 < Date.now();
  } catch { return true; }
}

export function useAuth(): void {
  const router = useRouter();
  useEffect(() => {
    const token = localStorage.getItem('zs_token');
    if (!token || isExpired(token)) {
      localStorage.removeItem('zs_token');
      localStorage.removeItem('zs_user');
      router.replace('/login');
    }
  }, [router]);
}

export function usePlayerAuth(): void {
  const router = useRouter();
  useEffect(() => {
    const token = localStorage.getItem('zs_player_token');
    if (!token || isExpired(token)) {
      localStorage.removeItem('zs_player_token');
      localStorage.removeItem('zs_player');
      router.replace('/login?mode=player');
    }
  }, [router]);
}

export function logout(): void {
  localStorage.removeItem('zs_token');
  localStorage.removeItem('zs_user');
  window.location.href = '/login';
}

export function playerLogout(): void {
  localStorage.removeItem('zs_player_token');
  localStorage.removeItem('zs_player');
  window.location.href = '/login?mode=player';
}
