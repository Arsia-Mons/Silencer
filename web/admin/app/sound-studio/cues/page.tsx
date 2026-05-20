'use client';
import { useEffect } from 'react';
import { useRouter } from 'next/navigation';

export default function SoundCueRedirectPage() {
  const router = useRouter();

  useEffect(() => {
    router.replace('/sound-studio?tab=cues');
  }, [router]);

  return null;
}
