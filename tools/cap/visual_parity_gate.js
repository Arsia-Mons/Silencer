export const meta = {
  name: 'visual-parity-gate',
  description: 'Rigorous multi-critic visual parity gate: each critic OPENS the shipped render AND the origin golden, compares them region-by-region, hunts shadcn/SaaS drift, then a synthesizer returns PASS/FAIL + ranked actionable fixes. Pixdiff is passed in; the critics are the visual eyes.',
  phases: [
    { title: 'Critique', detail: 'adversarial visual critics compare render vs golden (each reads BOTH images)' },
    { title: 'CodeHygiene', detail: 'review the diff for bloat comments + overengineering + un-idiomatic code' },
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
const checklist = a.checklist || '(no per-screen checklist provided — derive the target from the golden image itself)'
const codeFiles = a.files || '' // space-separated paths to scope the code-hygiene diff; empty = whole working tree

if (!render || !golden) throw new Error('visual-parity-gate: args.render and args.golden must be absolute PNG paths')

const RUBRIC = `You are validating that a MIGRATED cppx UI render is a 100% VISUAL MATCH to the ORIGINAL origin/main golden for the Silencer game screen "${screen}". The goal is byte-for-byte VISUAL parity with the original, NOT a tasteful modern redesign.

GROUND TRUTH ABOUT THE ORIGINAL LOOK (never "modernize" it): a DENSE, MULTI-COLOR green-phosphor HUD/console — emphatically NOT a modern SaaS/shadcn dashboard.
- Multi-color text, NOT uniform green: the "Silencer" brand is RED (~rgb 152,28,28); the version "v.00058" is AMBER (~140,64,8); agent NAMES are BLUE (~0,52,152 cornflower); body/labels/most button text are GREEN (~24,124,20); agency emblems are colored sprites (e.g. Noxis = a blue crystal). If the render paints these all green, that is a CRITICAL failure.
- Backdrops are baked sprites: the lobby cluster (lobby/create_game/game_staging) uses a DIM Mars + circuit-board HUD texture (nearly black with faint green traces). The menu/character-create screens use a starfield + a (brighter, but smooth) Mars. Backdrops are full-bleed, never flat black, never banded/striped.
- Panels are CONNECTED ~1px green hairline frames tiling edge-to-edge with small seams, NOT spaced rounded cards with gaps/gutters/drop-shadows/large radii.
- Buttons are sprite green ovals (menus) or thin-bordered rectangles (login); section titles sit inside baked pill-notch headers; type is condensed chunky upscaled-bitmap, NOT smooth modern sans, and not oversized.

MANDATORY METHOD: (1) Read the GOLDEN image at ${golden} (the origin reference, native 960x720). (2) Read the RENDER image at ${render} (a native HIGH-DEF cppx capture at 1920x1440 — the UI scales uniformly from a 640x480 logical base, so this is the SAME layout at higher device scale, just crisper; it is NOT an upscaled fake). The render being higher-resolution/crisper than the golden is EXPECTED — do NOT flag that as a defect; judge layout/proportion/color/type only. (3) Compare them region by region by relative position (both cover the same composition). You MUST cite at least 4 specific regions describing what you saw in EACH image. A discrepancy that does not describe what you actually saw in BOTH images is invalid and will be discarded.

ZERO TOLERANCE for shadcn/SaaS drift: if the render flattens the multi-color palette to uniform green, uses spaced rounded cards with gaps instead of connected hairline panels, oversized/modern type, flat fills instead of sprite chrome/backdrops, or otherwise reads as a modern redesign rather than the gritty original — that is an automatic FAIL, even if it looks "cleaner".

Per-screen target checklist (from a human whole-composition audit of the golden):
${checklist}`

const LENSES = [
  { key: 'palette', focus: 'COLOR & PALETTE. Verify every text/element hue against the golden: brand RED, version AMBER, agent names BLUE, body/labels GREEN, agency emblems colored sprites, backdrop tint. Flag ANY uniform-green flattening or wrong hue as HIGH severity. Sample-compare specific glyphs in both images.' },
  { key: 'layout', focus: 'LAYOUT & GEOMETRY. Panel positions, sizes, spacing, alignment, element placement, ordering, and whether panels are CONNECTED hairline frames vs spaced cards. Flag misplaced/missing/extra regions, wrong proportions, content not filling its frame, content clipping frame borders.' },
  { key: 'chrome', focus: 'CHROME / SPRITES / BACKDROP. Is the correct backdrop present and correct brightness (dim Mars + circuit texture for the lobby cluster; smooth starfield+Mars for menu/cc — no banding/stripes)? Sprite oval/rect buttons, pill-notch title headers, the agency-emblem portrait (a colored sprite, not a flat box), scrollbar rails. Flag flat color fills that replace sprite art.' },
  { key: 'type', focus: 'TYPOGRAPHY. Face, size, weight, casing, letterspacing for each text element vs golden. Chunky upscaled-bitmap vs smooth modern. Flag oversized headings (e.g. a giant brand title) and any modern sans drift.' },
  { key: 'adversary', focus: 'ANTI-DRIFT ADVERSARY. Assume the render is a modern-SaaS redesign and try to PROVE it. Hunt for spaced cards, gaps/gutters, large corner radii, uniform green, oversized type, flat fills, missing density, missing backdrops. Default your verdict to FAIL unless you genuinely cannot find drift.' },
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
          kind: { type: 'string', enum: ['bloat-comment', 'overengineering', 'non-idiomatic', 'dead-code', 'other'] },
          location: { type: 'string', description: 'file:line or quoted snippet' },
          detail: { type: 'string' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
  },
}
const codeReview = await agent(
  `You are a 2026 senior reviewer enforcing clean, idiomatic component code (shadcn/React sensibility ported to this C++/cppx UI). Inspect the working-tree diff: run \`git --no-pager diff -- ${codeFiles || '.'}\` (also \`git --no-pager diff --staged -- ${codeFiles || '.'}\`) with the Bash tool.\n\nFlag, with ZERO tolerance for noise:\n1. bloat-comment: comments that restate what the code already says, narrate the obvious, re-explain a token's name, or pad with backstory. Quote each offending comment verbatim. A comment earns its place ONLY if it explains a non-obvious WHY. Be strict — the codebase owner explicitly does not want comment bloat.\n2. overengineering: needless abstraction, single-use indirection, speculative flexibility/config, defensive handling of impossible states.\n3. non-idiomatic: patterns that fight the component model, copy-paste that should compose, magic numbers that should be tokens.\n4. dead-code.\n\nReturn verdict NEEDS_WORK if any high/medium finding exists. Only review ADDED/CHANGED lines in the diff, not pre-existing code.`,
  { label: 'code-hygiene', phase: 'CodeHygiene', schema: CODE },
)

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
  `Synthesize ${valid.length} independent VISUAL critic verdicts plus a CODE-hygiene review for the Silencer screen "${screen}" (numeric pixdiff vs golden = ${pix}%).\n\nVisual critic JSON:\n${JSON.stringify(valid)}\n\nCode-hygiene JSON:\n${JSON.stringify(codeReview)}\n\nRules: overall = PASS ONLY IF no visual critic returned FAIL AND there are zero high-severity visual discrepancies. Dedupe overlapping discrepancies across critics; rank by severity; produce an ACTIONABLE ordered top_fixes list phrased as concrete edits (which element, what to change). Be ruthless about shadcn/SaaS drift: if any critic flagged palette-flattening, spaced cards, oversized/modern type, or missing sprite backdrops, overall MUST be FAIL. Fold any high/medium code-hygiene findings (bloat comments, overengineering) into top_fixes too. Keep pixdiff as the passed-in number string.`,
  { label: 'synthesize', phase: 'Synthesize', schema: SYNTH },
)

return { screen, pixdiff: pix, overall: synth && synth.overall, top_fixes: (synth && synth.top_fixes) || [], summary: synth && synth.summary, critics: valid, code: codeReview }
