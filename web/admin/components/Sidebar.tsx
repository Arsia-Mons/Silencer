'use client';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { logout } from '../lib/auth';
import { useMemo } from 'react';

interface Props {
  wsConnected?: boolean;
}

const ROLE_RANK: Record<string, number> = { viewer: 0, moderator: 1, manager: 2, admin: 3, superadmin: 4 };

function getMyRank(): number {
  if (typeof window === 'undefined') return -1;
  try {
    const payload = JSON.parse(atob(localStorage.getItem('zs_token')?.split('.')[1] || ''));
    return ROLE_RANK[payload.role as string] ?? -1;
  } catch { return -1; }
}

const NAV = [
  { href: '/dashboard',       label: '[ LIVE SESSIONS ]',   icon: '◉', minRank: 0 },
  { href: '/players',         label: '[ PLAYERS ]',          icon: '◈', minRank: 0 },
  { href: '/audit',           label: '[ AUDIT LOG ]',         icon: '◧', minRank: 0 },
  { href: '/users',           label: '[ USER MGMT ]',         icon: '⬡', minRank: 3 },
  { href: '/actors',          label: '[ ACTOR EDITOR ]',      icon: '◉', minRank: 3 },
  { href: '/behavior-trees',  label: '[ BEHAVIOR TREES ]',    icon: '◬', minRank: 3 },
  { href: '/designer',        label: '[ MAP DESIGNER ]',      icon: '◫', minRank: 3 },
  { href: '/sound-studio',    label: '[ SOUND STUDIO ]',      icon: '♪', minRank: 3 },
  { href: '/gas',             label: '[ GAS EDITOR ]',         icon: '⚡', minRank: 3 },
  { href: '/sprites',         label: '[ SPRITES ]',            icon: '◈', minRank: 3 },
  { href: '/weapons',         label: '[ WEAPONS ]',            icon: '⚔', minRank: 3 },
  { href: '/items',           label: '[ ITEMS ]',              icon: '⊟', minRank: 3 },
  { href: '/vfx',             label: '[ VFX EDITOR ]',          icon: '✦', minRank: 3 },
  { href: '/health',          label: '[ SERVER HEALTH ]',     icon: '◎', minRank: 0 },
  { href: '/changelog',       label: '[ CHANGELOG ]',         icon: '◑', minRank: 0 },
];

export default function Sidebar({ wsConnected }: Props) {
  const path = usePathname();
  const rank = useMemo(() => getMyRank(), []);
  const visibleNav = NAV.filter(n => rank >= n.minRank);

  return (
    <aside className="w-56 min-h-screen bg-game-bgCard border-r border-game-border flex flex-col">
      {/* Logo */}
      <div className="px-4 py-5 border-b border-game-border">
        <img src="/logo.png" alt="Silencer" className="h-10 w-auto mb-1" />
        <div className="text-game-textDim text-xs tracking-widest mt-0.5">ADMIN CONSOLE</div>
      </div>

      {/* Live indicator — only shown on pages that use WebSocket */}
      {wsConnected !== undefined && (
        <div className="px-4 py-2 border-b border-game-border flex items-center gap-2">
          <span className={`w-2 h-2 rounded-full ${wsConnected ? 'bg-game-primary animate-pulse' : 'bg-game-danger'}`} />
          <span className="text-xs text-game-textDim">{wsConnected ? 'LIVE' : 'OFFLINE'}</span>
        </div>
      )}

      {/* Nav */}
      <nav className="flex-1 py-4">
        {visibleNav.map(({ href, label, icon }) => {
          const active = path.startsWith(href);
          return (
            <Link key={href} href={href}
              className={`flex items-center gap-3 px-4 py-3 text-xs font-mono tracking-wide transition-colors
                ${active
                  ? 'bg-game-dark text-game-primary border-r-2 border-game-primary'
                  : 'text-game-textDim hover:text-game-text hover:bg-game-bgHover'}`}>
              <span className="text-base">{icon}</span>
              {label}
            </Link>
          );
        })}
      </nav>

      {/* Logout */}
      <button onClick={logout}
        className="mx-4 mb-4 px-3 py-2 text-xs font-mono text-game-muted border border-game-border rounded hover:border-game-danger hover:text-game-danger transition-colors">
        [ LOGOUT ]
      </button>
    </aside>
  );
}
