# LESSONS.md — one lesson per entry, why it mattered

- **Hot tiles cluster by family — diagnose the family before fixing screens.** Iteration 0:
  13/13 screens failed the tile gate, but the worst tiles in 8 of them are the same Mars
  backdrop region. Measuring one tile's row profile revealed a single systemic cause
  (striping arithmetic), not 13 layout bugs. Always compare worst-tile coords across screens
  before opening per-screen work.
- **Region means can match while tiles read 30%.** The tolerant gate at 640×360 still sees
  scanline phase mismatch as huge diffs even when 78px-region means agree within ±2 RGB.
  A hot tile therefore doesn't always mean misplaced/wrong-colored content — dump per-row
  profiles (numpy) before trusting the % as a layout signal.
- **The pixdiff noise-ceiling calibration (~9.6%) did not include real backdrop striping**
  (reads 20–33%/tile). Don't waive these tiles (rule: >10% never waivable) and don't relax
  the gate; resolve the striping design question explicitly instead.
- **cap_menus.sh/cap_lobby.sh boot their own Silencer instances** — don't run them
  concurrently with the e2e suite; serialize capture → e2e.
- **`pkill -f Silencer` (the goal's build preamble) kills EVERY instance** — including
  e2e instances owned by parallel repair agents. While subagents run e2e, skip the pkill
  or scope it to the capture's own PID.
- **Origin's image fit modes are framebuffer-relative.** PackImage(6,0)=cover happens in
  origin's 640×480 element space, and the framebuffer then stretches non-uniformly to the
  window — so at the glass, every full-bleed backdrop is a STRETCH. The migration
  transcribed "cover" literally and got uniform 3× scaling + crop = wrong geometry AND
  wrong scanline arithmetic on 8 screens. When porting legacy draw flags, trace the full
  chain to the glass, not the flag name.
