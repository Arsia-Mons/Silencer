'use client';
import { create } from 'zustand';

export interface LightDef {
  id: string;
  name: string;
  description: string;
  bank: number;
  frame: number;
  radius: number;
  intensity: number;
  defaultColor?: string;
}

interface LightsStore {
  lights: LightDef[];
  loaded: boolean;
  load: () => Promise<void>;
}

export const useLightsStore = create<LightsStore>((set, get) => ({
  lights: [],
  loaded: false,
  load: async () => {
    if (get().loaded) return;
    try {
      const res = await fetch('/api/gas/lights');
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      set({ lights: data.lights ?? [], loaded: true });
    } catch (e) {
      console.error('[lights-store] failed to load lights.json', e);
      set({ loaded: true });
    }
  },
}));
