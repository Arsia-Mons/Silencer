interface Props {
  label: string;
  value?: string | number;
  sub?: string;
  color?: 'primary' | 'danger' | 'warning' | 'info';
}

export default function StatCard({ label, value, sub, color = 'primary' }: Props) {
  const colors: Record<string, string> = {
    primary: 'border-game-primary text-game-primary',
    danger:  'border-game-danger  text-game-danger',
    warning: 'border-game-warning text-game-warning',
    info:    'border-game-info    text-game-info',
  };
  return (
    <div className={`bg-game-bgCard border ${colors[color]} rounded p-4`}>
      <div className="text-xs text-game-textDim tracking-widest mb-1">{label}</div>
      <div className="text-3xl font-bold font-mono">{value ?? '—'}</div>
      {sub && <div className="text-xs text-game-muted mt-1">{sub}</div>}
    </div>
  );
}
