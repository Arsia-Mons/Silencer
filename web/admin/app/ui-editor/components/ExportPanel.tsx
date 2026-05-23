export function ExportPanel({ exportText }: {
  exportText: string;
}) {
  return (
    <div className="h-52 border-t border-game-border bg-game-bgCard/90 flex flex-col">
      <div className="px-4 py-2 border-b border-game-border flex items-center gap-2">
        <div className="px-3 py-1 text-[11px] tracking-widest border border-game-primary text-game-primary">
          JSON
        </div>
      </div>
      <textarea
        readOnly
        value={exportText}
        className="min-h-0 flex-1 resize-none bg-black/60 p-4 font-mono text-[11px] leading-5 text-game-textDim outline-none"
      />
    </div>
  );
}
