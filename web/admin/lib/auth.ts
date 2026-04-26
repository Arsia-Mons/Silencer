'use client';
import { useRouter } from 'next/navigation';
import { useEffect } from 'react';

export function useAuth(): void {
  const router = useRouter();
  useEffect(() => {
    if (!localStorage.getItem('zs_token')) router.replace('/login');
  }, [router]);
}

export function usePlayerAuth(): void {
  const router = useRouter();
  useEffect(() => {
    if (!localStorage.getItem('zs_player_token')) router.replace('/login?mode=player');
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
