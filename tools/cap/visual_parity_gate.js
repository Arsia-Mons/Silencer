export const meta = {
  name: 'visual-parity-gate',
  description: 'Rigorous multi-critic visual parity gate: each critic OPENS the shipped render AND the origin golden, compares them region-by-region, hunts shadcn/SaaS drift, then a synthesizer returns PASS/FAIL + ranked actionable fixes. Pixdiff is passed in; the critics are the visual eyes.',
  phases: [
    { title: 'Critique', detail: 'adversarial visual critics compare render vs golden (each reads BOTH images)' },
    { title: 'CodeHygiene', detail: 'architecture critic panel: hygiene, composition, state/data-flow, control-flow lenses over the diff' },
    { title: 'Synthesize', detail: 'dedupe + rank + overall PASS/FAIL verdict' },
  ],
}

let a = args
if (typeof a === 'string') { try { a = JSON.parse(a) } catch (e) { a = {} } }
a = a || {}
const screen = a.screen || 'screen'
const render = a.render
const golden = a.golden
const pix = (a.pixdiff != null) ? String(a.pixdiff) : 'n/a'
// Tolerant metric (tools/cap/pixdiff_tolerant.py, recalibrated 2026-06-11):
// PASS requires global < 1% AND zero ~80px tiles over 5% at the 640x360
// working res. NOT raw byte-exact — tools/pixdiff is ~6% (black dialog) to
// ~38% (backdrop screens) even at perfect visual parity, because origin
// point-upscales (scanline striping) while cppx renders crisp. A high RAW
// pixdiff is NOT by itself a parity failure; the critics' visual judgement
// + the tolerant tile gate are authoritative.
const pixTolerant = (a.pixdiff_tolerant != null) ? String(a.pixdiff_tolerant) : 'n/a'
const checklist = a.checklist || '(no per-screen checklist provided — derive the target from the golden image itself)'
const codeFiles = a.files || '' // space-separated paths to scope the code-hygiene diff; empty = whole working tree

if (!render || !golden) throw new Error('visual-parity-gate: args.render and args.golden must be absolute PNG paths')

const RUBRIC = `You are validating that a MIGRATED cppx UI render is a 100% VISUAL MATCH to the ORIGINAL origin/main golden for the Silencer game screen "${screen}". The goal is byte-for-byte VISUAL parity with the original, NOT a tasteful modern redesign.

GROUND TRUTH ABOUT THE ORIGINAL LOOK (never "modernize" it): a DENSE, MULTI-COLOR green-phosphor HUD/console — emphatically NOT a modern SaaS/shadcn dashboard.
- Multi-color text, NOT uniform green: the "Silencer" brand is RED (~rgb 152,28,28); the version "v.00058" is AMBER (~140,64,8); agent NAMES are BLUE (~0,52,152 cornflower); body/labels/most button text are GREEN (~24,124,20); agency emblems are colored sprites (e.g. Noxis = a blue crystal). If the render paints these all green, that is a CRITICAL failure.
- Backdrops are baked sprites: the lobby cluster (lobby/create_game/game_staging) uses a DIM Mars + circuit-board HUD texture (nearly black with faint green traces). The menu/character-create screens use a starfield + a (brighter, but smooth) Mars. Backdrops are full-bleed, never flat black, never banded/striped.
- Panels are CONNECTED ~1px green hairline frames tiling edge-to-edge with small seams, NOT spaced rounded cards with gaps/gutters/drop-shadows/large radii.
- Buttons are sprite green ovals (menus) or thin-bordered rectangles (login); section titles sit inside baked pill-notch headers; type is condensed chunky upscaled-bitmap, NOT smooth modern sans, and not oversized.

MANDATORY METHOD: (1) Read the GOLDEN image at ${golden} (the origin/main reference, captured at 1920x1080). (2) Read the RENDER image at ${render} (the cppx capture, also at 1920x1080). Both are the SAME resolution and the SAME composition — the cppx UI lays out against a height-720 logical canvas and scales uniformly to 1920x1080, exactly as origin does. The cppx render may be crisper (text re-rasterized at device scale) — do NOT flag crispness as a defect; judge layout/proportion/color/type only. (3) Compare them region by region by relative position (both cover the same composition). You MUST cite at least 4 specific regions describing what you saw in EACH image. A discrepancy that does not describe what you actually saw in BOTH images is invalid and will be discarded.

ZERO TOLERANCE for shadcn/SaaS drift: if the render flattens the multi-color palette to uniform green, uses spaced rounded cards with gaps instead of connected hairline panels, oversized/modern type, flat fills instead of sprite chrome/backdrops, or otherwise reads as a modern redesign rather than the gritty original — that is an automatic FAIL, even if it looks "cleaner".

THIS IS A FAITHFUL-RECREATION TASK, NOT A LOOKALIKE TASK. The golden is rendered from the ORIGINAL game's baked SPRITE chrome — ornate dialog/panel frames, beveled/double-stroked button frames, stipple/dither fills, inset field wells, and notch/cutout cells. A render that reproduces this chrome with FLAT VECTOR fills, single uniform hairline borders, plain dividers, or rounded primitive boxes — EVEN IF it is dimensionally close and lands at low pixdiff — is a VECTOR FORGERY of the sprite, and that is a HIGH-severity fidelity FAILURE. The render is required to REUSE THE ORIGINAL SPRITE ART (bake + draw the actual sprite bank/index), not approximate it. Specific tells of a forgery to hunt for and rate HIGH: (a) a frame/border drawn as a FLAT uniform vector line or primitive rectangle that LACKS the sprite's internal structure (its bevel, double-band, baked wells); (b) buttons that are flat single-outline fills where the golden has a beveled/multi-band sprite; (c) panel interiors that are perfectly flat where the golden shows baked stipple/dither texture; (d) rounded corners where the sprite has square/notched corners; (e) missing baked wells, notch cells, scrollbar rails, or pill-notch title cutouts that exist in the golden. Do NOT excuse these as "minor polish" — collectively they mean the screen was reinvented rather than recreated.

CRISPNESS EXCEPTION (do not over-fail faithful sprites): the cppx renders CRISP at device scale, while origin POINT-UPSCALES its low-res framebuffer — so the golden has soft/blurred edges, scanline striping, and a soft border bloom that the cppx will not reproduce. A FAITHFUL recreation that DOES reuse the original sprite art will therefore look CRISPER, and may have a TIGHTER or BRIGHTER glow/halo, than the golden. That is NOT a forgery and NOT a defect — do NOT FAIL on crispness, glow-softness, edge-bloom, or slightly-higher brightness ALONE. The forgery test is whether the original SPRITE ART is present (its frame structure / bevel / baked wells / stipple are visible), NOT whether the edge is as soft as origin's upscale. Only call FORGERY when the chrome is genuinely a flat vector primitive standing in for the sprite.

Per-screen target checklist (from a human whole-composition audit of the golden):
${checklist}`

const LENSES = [
  { key: 'palette', focus: 'COLOR & PALETTE. Verify every text/element hue against the golden: brand RED, version AMBER, agent names BLUE, body/labels GREEN, agency emblems colored sprites, backdrop tint. Flag ANY uniform-green flattening or wrong hue as HIGH severity. Sample-compare specific glyphs in both images.' },
  { key: 'layout', focus: 'LAYOUT & GEOMETRY. Panel positions, sizes, spacing, alignment, element placement, ordering, and whether panels are CONNECTED hairline frames vs spaced cards. Flag misplaced/missing/extra regions, wrong proportions, content not filling its frame, content clipping frame borders.' },
  { key: 'chrome', focus: 'CHROME / SPRITES / BACKDROP. Is the correct backdrop present and correct brightness (dim Mars + circuit texture for the lobby cluster; smooth starfield+Mars for menu/cc — no banding/stripes)? Sprite oval/rect buttons, pill-notch title headers, the agency-emblem portrait (a colored sprite, not a flat box), scrollbar rails. Flag flat color fills that replace sprite art.' },
  { key: 'type', focus: 'TYPOGRAPHY. Face, size, weight, casing, letterspacing for each text element vs golden. Chunky upscaled-bitmap vs smooth modern. Flag oversized headings (e.g. a giant brand title) and any modern sans drift.' },
  { key: 'adversary', focus: 'ANTI-DRIFT ADVERSARY. Assume the render is a modern-SaaS redesign and try to PROVE it. Hunt for spaced cards, gaps/gutters, large corner radii, uniform green, oversized type, flat fills, missing density, missing backdrops. Default your verdict to FAIL unless you genuinely cannot find drift.' },
  { key: 'fidelity', focus: 'SPRITE-FIDELITY / FORGERY DETECTION. Your job is to decide whether the render REUSES the original baked SPRITE chrome or is a VECTOR FORGERY of it. For every chrome element (the outer frame, panel borders, button frames, title-bar/notch headers, field wells, scrollbar rails, portrait/emblem, backdrop), judge: is the render the actual sprite art (beveled/multi-band buttons, stipple/dither fills, square notched corners, baked wells, the sprite frame structure), or a flat vector approximation (a uniform hairline/box border, flat single-outline buttons, perfectly smooth fills with no baked texture, rounded primitive corners, plain dividers, absent wells)? A chrome element that is a flat vector lookalike replacing the sprite is a HIGH-severity discrepancy — say so per element; if MOST chrome is vector-forged, FAIL with high confidence. BUT apply the CRISPNESS EXCEPTION: a faithful sprite that merely renders crisper, with a tighter/brighter glow, or slightly higher brightness than origin\'s soft upscale is NOT a forgery — do not flag crispness/glow-softness/brightness as a defect. The test is the PRESENCE of the sprite structure (bevel/wells/stipple/frame), not edge softness. If the original sprite art IS present everywhere (just crisp), PASS.' },
]

const VERDICT = {
  type: 'object', additionalProperties: false,
  required: ['lens', 'verdict', 'confidence', 'regions_checked', 'discrepancies'],
  properties: {
    lens: { type: 'string' },
    verdict: { type: 'string', enum: ['PASS', 'FAIL'] },
    confidence: { type: 'number', description: '0..1' },
    regions_checked: { type: 'array', items: { type: 'string' }, description: 'coordinates/regions you compared in BOTH images, with what you saw in each' },
    discrepancies: {
      type: 'array', items: {
        type: 'object', additionalProperties: false,
        required: ['region', 'expected', 'actual', 'severity'],
        properties: {
          region: { type: 'string' },
          expected: { type: 'string', description: 'what the golden shows' },
          actual: { type: 'string', description: 'what the render shows' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
  },
}

phase('Critique')
const verdicts = await parallel(LENSES.map((l) => () =>
  agent(
    `${RUBRIC}\n\nYOUR ASSIGNED LENS — ${l.key}:\n${l.focus}\n\nStay in your lens but you may note an obvious out-of-lens defect. Return your structured verdict. Be specific and adversarial; "looks fine" without per-region evidence from BOTH images is not acceptable.`,
    { label: `critic:${l.key}`, phase: 'Critique', schema: VERDICT },
  ),
))
const valid = verdicts.filter(Boolean)

phase('CodeHygiene')
const CODE = {
  type: 'object', additionalProperties: false,
  required: ['verdict', 'findings'],
  properties: {
    verdict: { type: 'string', enum: ['CLEAN', 'NEEDS_WORK'] },
    findings: {
      type: 'array', items: {
        type: 'object', additionalProperties: false,
        required: ['kind', 'location', 'detail', 'severity'],
        properties: {
          kind: { type: 'string', enum: ['bloat-comment', 'overengineering', 'non-idiomatic', 'dead-code', 'composition', 'prop-drilling', 'state-ownership', 'control-flow', 'other'] },
          location: { type: 'string', description: 'file:line or quoted snippet' },
          detail: { type: 'string' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
  },
}
const CODE_PREAMBLE = `You are a 2026 senior reviewer enforcing clean, idiomatic component code (shadcn/React sensibility ported to this C++/cppx UI). Inspect the working-tree diff: run \`git --no-pager diff -- ${codeFiles || '.'}\` (also \`git --no-pager diff --staged -- ${codeFiles || '.'}\`) with the Bash tool. Only review ADDED/CHANGED lines, not pre-existing code. The engine-idiom golden is /Users/hv/repos/ui — when unsure whether a pattern is idiomatic cppx, read how that repo's own components do it. Deterministic smells (raw Color{} paint, >6-case switches, fat props/signatures, god views, conditional hooks) are already machine-checked by clients/silencer/tools/react_architecture_guard.py — do not re-litigate those mechanically; focus on the judgement calls in your lens. Return verdict NEEDS_WORK if any high/medium finding exists. ZERO tolerance for noise: every finding cites file:line or a verbatim snippet.`

const CODE_LENSES = [
  { key: 'hygiene', focus: 'HYGIENE. (1) bloat-comment: comments that restate the code, narrate the obvious, or pad with backstory — quote each verbatim; a comment earns its place ONLY by explaining a non-obvious WHY (the owner explicitly rejects comment bloat). (2) overengineering: needless abstraction, single-use indirection, speculative flexibility/config, defensive handling of impossible states. (3) dead-code.' },
  { key: 'composition', focus: 'COMPOSITION. Does each screen read top-down as a tree of named primitives, or as procedural orchestration? Repeated structure that should be extracted into the shared primitives (clients/silencer/src/client/ui/components/) instead of copy-pasted? One-off styling inline where a variant of an existing primitive exists? Sections of a view function that are really components wanting out?' },
  { key: 'state', focus: 'STATE & DATA FLOW. State lives in the narrowest owner; derived values are computed at build, never stored-and-synced; cross-cutting state (focus, session, settings, theme) flows through providers/capability hooks (use_session/use_settings/...), not threaded through every props struct (prop drilling). Hooks called unconditionally at the top of the view; effects/intent closures for side-effects — screens never reach gameplay except through hook intent closures. Flag any pass-through prop that crosses 2+ levels untouched.' },
  { key: 'controlflow', focus: 'CONTROL FLOW. Variants/modes dispatched via data (token maps, small local variant->token switches are the documented convention) — but flag MVC-shaped god-dispatchers: a function that knows every screen/mode, switch pyramids growing a case per feature, boolean-flag parameters that fork a function into two behaviors, deeply nested conditionals that should be early returns or separate components.' },
]

const codeReviews = (await parallel(CODE_LENSES.map((l) => () =>
  agent(`${CODE_PREAMBLE}\n\nYOUR ASSIGNED LENS — ${l.key}:\n${l.focus}\n\nStay in your lens but you may note an obvious out-of-lens defect.`,
    { label: `code:${l.key}`, phase: 'CodeHygiene', schema: CODE }),
))).filter(Boolean)
const codeReview = {
  verdict: codeReviews.some((r) => r.verdict === 'NEEDS_WORK') ? 'NEEDS_WORK' : 'CLEAN',
  findings: codeReviews.flatMap((r) => r.findings),
}

phase('Synthesize')
const SYNTH = {
  type: 'object', additionalProperties: false,
  required: ['overall', 'pixdiff', 'top_fixes', 'summary'],
  properties: {
    overall: { type: 'string', enum: ['PASS', 'FAIL'] },
    pixdiff: { type: 'string' },
    top_fixes: {
      type: 'array', items: {
        type: 'object', additionalProperties: false,
        required: ['fix', 'region', 'severity'],
        properties: {
          fix: { type: 'string', description: 'a concrete edit to make' },
          region: { type: 'string' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
    summary: { type: 'string' },
  },
}
const synth = await agent(
  `Synthesize ${valid.length} independent VISUAL critic verdicts plus a CODE-hygiene review for the Silencer screen "${screen}" (raw byte-exact pixdiff vs golden = ${pix}%; tolerant/perceptual pixdiff = ${pixTolerant}%).\n\nVisual critic JSON:\n${JSON.stringify(valid)}\n\nCode-hygiene JSON:\n${JSON.stringify(codeReview)}\n\nRules: overall = PASS ONLY IF no visual critic returned FAIL AND there are zero high-severity visual discrepancies. The pixdiff numbers are SECONDARY signals — the RAW byte-exact pixdiff is ~6% (black dialog) to ~38% (backdrop screens) even at perfect visual parity because origin point-upscales (scanline striping) while cppx renders crisp, so do NOT fail on raw pixdiff alone; the user's gate is the TOLERANT tile metric (global <1% AND no ~80px tile >5% — pixdiff_tolerant.py prints the verdict). Dedupe overlapping discrepancies across critics; rank by severity; produce an ACTIONABLE ordered top_fixes list phrased as concrete edits (which element, what to change). Be ruthless about shadcn/SaaS drift: if any critic flagged palette-flattening, spaced cards, oversized/modern type, or missing sprite backdrops, overall MUST be FAIL.
FIDELITY / VECTOR-FORGERY RULES (do not let a lookalike pass, but do not over-fail faithful sprites):
- If the fidelity critic (or any critic) flagged a chrome element as a flat VECTOR approximation REPLACING the original SPRITE (a uniform hairline/box border lacking the sprite's structure, flat single-outline buttons vs a beveled sprite, smooth fills vs baked stipple/dither, rounded primitive corners vs square/notched, missing baked wells/notch-cells/rails), overall MUST be FAIL — even if pixdiff is low and other critics passed. A vector forgery of sprite chrome is NOT parity.
- CRISPNESS EXCEPTION: do NOT FAIL when the render DOES use the original sprite art but is merely CRISPER, has a TIGHTER/BRIGHTER glow, or is slightly brighter than origin's soft point-upscaled output. That edge-softness/brightness gap is an inherent renderer difference (origin upscales a low-res framebuffer; cppx renders crisp) and the rubric forbids flagging crispness. A finding whose ONLY substance is "no soft glow / harder edge / brighter frame" on chrome that is otherwise the correct sprite is NOT a forgery — drop it to low and do not let it force FAIL.
- CUMULATIVE AGGREGATION: if the SAME element is flagged by >=2 critics as a vector forgery (missing the sprite's bevel/stipple/well STRUCTURE, not merely its softness) — even when each rated it LOW/MEDIUM — treat it as HIGH and FAIL. Death-by-a-thousand-LOW-cuts of genuine structural loss is a reinvention; but cumulative crispness/glow complaints do NOT aggregate to FAIL.
- ROOT-CAUSE top_fixes: when the defect is a vector forgery, the fix must name the faithful remedy — "bake and draw the original sprite (bank N idx M) for this chrome instead of the vector box", NOT a symptom patch like "add a glow to the vector border". Prefer reusing the original sprite art over hand-tuning a vector approximation.
Fold any high/medium code-hygiene findings (bloat comments, overengineering) into top_fixes too. Keep pixdiff as the passed-in number string.`,
  { label: 'synthesize', phase: 'Synthesize', schema: SYNTH },
)

return { screen, pixdiff: pix, overall: synth && synth.overall, top_fixes: (synth && synth.top_fixes) || [], summary: synth && synth.summary, critics: valid, code: codeReview }
