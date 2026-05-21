'use client';
import { useEffect } from 'react';
import { useRouter } from 'next/navigation';

export default function SoundCueRedirectPage() {
  const router = useRouter();

  useEffect(() => {
    const cue = new URLSearchParams(window.location.search).get('cue');
    const dest = cue
      ? `/sound-studio?tab=cues&cue=${encodeURIComponent(cue)}`
      : '/sound-studio?tab=cues';
    router.replace(dest);
  }, [router]);

  return null;
}
