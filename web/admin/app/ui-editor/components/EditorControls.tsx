import { type ReactNode } from "react";

export function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <label className="block text-[11px] tracking-widest text-game-textDim">
      <span className="mb-1 block">{label}</span>
      {children}
    </label>
  );
}

export function TextInput({
  value,
  onChange,
}: {
  value: string;
  onChange: (value: string) => void;
}) {
  return (
    <input
      value={value}
      onChange={(event) => onChange(event.target.value)}
      className="w-full bg-game-bg border border-game-border px-2 py-1.5 text-game-text focus:outline-none focus:border-game-primary"
    />
  );
}

export function NumberInput({
  value,
  min,
  max,
  disabled,
  onChange,
}: {
  value: number;
  min: number;
  max: number;
  disabled?: boolean;
  onChange: (value: number) => void;
}) {
  return (
    <input
      value={value}
      disabled={disabled}
      type="number"
      min={min}
      max={max}
      onChange={(event) => onChange(Number(event.target.value))}
      className="w-full bg-game-bg border border-game-border px-2 py-1.5 text-game-text focus:outline-none focus:border-game-primary disabled:opacity-40"
    />
  );
}

export function Select({
  value,
  options,
  onChange,
}: {
  value: string;
  options: readonly string[];
  onChange: (value: string) => void;
}) {
  return (
    <select
      value={value}
      onChange={(event) => onChange(event.target.value)}
      className="w-full bg-game-bg border border-game-border px-2 py-1.5 text-game-text focus:outline-none focus:border-game-primary"
    >
      {options.map((option) => (
        <option key={option} value={option}>
          {option}
        </option>
      ))}
    </select>
  );
}

export function ToolbarButton({ children, onClick }: { children: ReactNode; onClick: () => void }) {
  return (
    <button
      onClick={onClick}
      className="border border-game-border px-3 py-2 text-[11px] tracking-widest text-game-textDim hover:border-game-primary hover:text-game-text"
    >
      {children}
    </button>
  );
}
