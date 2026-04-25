'use client';
import { useAuth } from '../../lib/auth.js';
import { useSocket } from '../../lib/socket.js';
import Sidebar from '../../components/Sidebar.js';
import { CHANGELOG, CATEGORY_META } from '../../lib/changelog.js';

function CategoryBadge({ category }) {
  const meta = CATEGORY_META[category] || CATEGORY_META.DASHBOARD;
  return (
    <span className={`inline-flex items-center gap-1 px-2 py-0.5 rounded text-[10px] font-mono font-bold tracking-widest border ${meta.color} ${meta.border} ${meta.bg}`}>
      {meta.icon} {meta.label}
    </span>
  );
}

function VersionBlock({ release }) {
  return (
    <div className="mb-10">
      <div className="flex items-baseline gap-4 mb-4 pb-2 border-b border-game-border">
        <span className="text-game-primary font-mono font-bold text-lg tracking-widest">{release.version}</span>
        <span className="text-game-textDim font-mono text-xs">{release.date}</span>
        <span className="text-game-text font-mono text-sm flex-1">{release.title}</span>
      </div>
      <div className="space-y-4 pl-2">
        {release.entries.map((entry) => {
          const meta = CATEGORY_META[entry.category] || CATEGORY_META.DASHBOARD;
          return (
            <div key={entry.category} className={`rounded border ${meta.border} ${meta.bg} p-4`}>
              <div className="mb-3">
                <CategoryBadge category={entry.category} />
              </div>
              <ul className="space-y-1.5">
                {entry.changes.map((change, i) => (
                  <li key={i} className="flex gap-2 text-xs font-mono text-game-text leading-relaxed">
                    <span className="text-game-muted shrink-0 mt-0.5">›</span>
                    <span>{change}</span>
                  </li>
                ))}
              </ul>
            </div>
          );
        })}
      </div>
    </div>
  );
}

export default function ChangelogPage() {
  useAuth();
  const wsConnected = useSocket({});
  const latest = CHANGELOG[0];

  return (
    <div className="flex min-h-screen">
      <Sidebar wsConnected={wsConnected} />
      <main className="flex-1 p-6 overflow-auto">
        <div className="flex items-start justify-between mb-6">
          <div>
            <h1 className="text-game-primary font-mono text-xl tracking-widest mb-1">◑ CHANGELOG</h1>
            <p className="text-game-textDim text-xs font-mono">
              CURRENT&nbsp;
              <span className="text-game-primary">{latest.version}</span>
              &nbsp;·&nbsp;{latest.date}
            </p>
          </div>
          <div className="flex gap-2 flex-wrap justify-end">
            {Object.values(CATEGORY_META).map((meta) => (
              <span key={meta.label} className={`inline-flex items-center gap-1 px-2 py-0.5 rounded text-[10px] font-mono border ${meta.color} ${meta.border} ${meta.bg}`}>
                {meta.icon} {meta.label}
              </span>
            ))}
          </div>
        </div>

        {CHANGELOG.map((release) => (
          <VersionBlock key={release.version} release={release} />
        ))}

        <p className="text-game-muted text-[10px] font-mono text-center mt-4 pb-2">
          SILENCER ADMIN CONSOLE — ALL TIMES UTC
        </p>
      </main>
    </div>
  );
}
