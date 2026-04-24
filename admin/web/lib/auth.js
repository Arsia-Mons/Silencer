'use client';
import { useRouter } from 'next/navigation';
import { useEffect } from 'react';

export function useAuth() {
  const router = useRouter();
  useEffect(() => {
    if (!localStorage.getItem('zs_token')) router.replace('/login');
  }, [router]);
}

export function usePlayerAuth() {
  const router = useRouter();
  useEffect(() => {
    if (!localStorage.getItem('zs_player_token')) router.replace('/login?mode=player');
  }, [router]);
}

export function logout() {
  localStorage.removeItem('zs_token');
  localStorage.removeItem('zs_user');
  window.location.href = '/login';
}

export function playerLogout() {
  localStorage.removeItem('zs_player_token');
  localStorage.removeItem('zs_player');
  window.location.href = '/login?mode=player';
}
