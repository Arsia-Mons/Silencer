export const meta = {
  name: 'gate-all',
  description: 'Fan-out visual-parity critic panel across every target screen at once: per-screen 5-lens critics (each opens render+golden) -> per-screen PASS/FAIL + ranked fixes, plus one global code-hygiene pass over the diff.',
  phases: [
    { title: 'Critique', detail: '5 visual critics per screen compare render vs golden region-by-region' },
    { title: 'CodeHygiene', detail: 'architecture critic panel over the working-tree diff: hygiene, composition, state/data-flow, control-flow' },
    { title: 'Report', detail: 'per-screen synthesis: PASS/FAIL + ranked top_fixes' },
  ],
}

let a = args
if (typeof a === 'string') { try { a = JSON.parse(a) } catch (e) { a = {} } }
a = a || {}
const screens = a.screens
if (!Array.isArray(screens) || !screens.length) throw new Error('gate-all: args.screens[] required ({screen,render,golden,pixdiff,checklist})')
const codeFiles = a.files || ''

const RUBRIC = (s) => `You are validating that a MIGRATED cppx UI render is a 100% VISUAL MATCH to the ORIGINAL origin/main golden for the Silencer game screen "${s.screen}". The goal is byte-for-byte VISUAL parity with the original, NOT a tasteful modern redesign.

GROUND TRUTH ABOUT THE ORIGINAL LOOK (never "modernize" it): a DENSE, MULTI-COLOR green-phosphor HUD/console — emphatically NOT a modern SaaS/shadcn dashboard.
- Multi-color text, NOT uniform green: the "Silencer" brand wordmark is RED (~rgb 152,28,28); the version "v.00058" is AMBER (~140,64,8); agent NAMES are BLUE (~40,96,200 cornflower); body/labels/most button text are GREEN (~24,124,20); agency emblems are colored sprites. If the render paints these all green, that is a CRITICAL failure.
- Backdrops are baked sprites: the lobby cluster (lobby/create_game/game_staging) uses a DIM Mars + circuit-board HUD texture (nearly black with faint green traces). The menu/character-create screens use a starfield + a (brighter, but smooth) Mars. Backdrops are full-bleed, never flat black, never banded/striped.
- Panels are CONNECTED ~1px green hairline frames tiling edge-to-edge with small seams, NOT spaced rounded cards with gaps/gutters/drop-shadows/large radii.
- Buttons are LARGE sprite green ovals/pills (menus) or thin-bordered rectangles (login); section titles sit inside baked pill-notch headers; type is condensed chunky upscaled-bitmap, NOT smooth modern sans, and not oversized.

MANDATORY METHOD: (1) Read the GOLDEN image at ${s.golden}. (2) Read the RENDER image at ${s.render}. (3) Compare region by region. If a region is hard to see, use the Bash tool with python3+PIL to crop/upscale that region of BOTH images (e.g. Image.open(p).crop((x,y,x2,y2)).resize((w*3,h*3))) and re-open. You MUST cite at least 4 specific regions (approx pixel coords; both images are 1920x1080) describing what you saw in EACH image. A discrepancy that does not describe what you actually saw in BOTH images is invalid.

ZERO TOLERANCE for shadcn/SaaS drift: uniform-green flattening, spaced rounded cards with gaps instead of connected hairline panels, oversized/modern type, flat fills instead of sprite chrome/backdrops, undersized buttons, or anything that reads as a modern redesign = automatic FAIL even if "cleaner".

Per-screen target checklist (human whole-composition audit; the golden image itself overrides this):
${s.checklist || '(derive the target entirely from the golden image)'}`

const LENSES = [
  { key: 'palette', focus: 'COLOR & PALETTE. Verify every text/element hue against the golden: brand RED, version AMBER, agent names BLUE, body/labels GREEN, agency emblems colored sprites, backdrop tint. Flag ANY uniform-green flattening or wrong hue as HIGH severity.' },
  { key: 'layout', focus: 'LAYOUT & GEOMETRY. Panel/button positions, sizes, spacing, alignment, ordering; CONNECTED hairline frames vs spaced cards. Flag misplaced/missing/extra regions, wrong proportions (esp. UNDERSIZED or too-tightly-stacked buttons), content not filling its frame, content clipping borders. Give golden vs render pixel coords/sizes.' },
  { key: 'chrome', focus: 'CHROME / SPRITES / BACKDROP. Correct backdrop present at correct brightness (dim Mars+circuit for lobby cluster; smooth starfield+Mars for menu/cc — no banding)? Sprite oval/rect buttons, pill-notch title headers, agency-emblem portrait (colored sprite not flat box), scrollbar rails, footer dome row. Flag flat fills replacing sprite art.' },
  { key: 'type', focus: 'TYPOGRAPHY. Face, size, weight, casing, letterspacing per text element vs golden. Chunky upscaled-bitmap vs smooth modern. Flag oversized/undersized headings and any modern-sans drift.' },
  { key: 'adversary', focus: 'ANTI-DRIFT ADVERSARY. Assume the render is a modern-SaaS redesign and PROVE it. Hunt spaced cards, gaps/gutters, large radii, uniform green, oversized/undersized type, flat fills, missing density, missing backdrops. Default verdict FAIL unless you genuinely cannot find drift.' },
]

const VERDICT = {
  type: 'object', additionalProperties: false,
  required: ['lens', 'verdict', 'confidence', 'regions_checked', 'discrepancies'],
  properties: {
    lens: { type: 'string' },
    verdict: { type: 'string', enum: ['PASS', 'FAIL'] },
    confidence: { type: 'number' },
    regions_checked: { type: 'array', items: { type: 'string' } },
    discrepancies: {
      type: 'array', items: {
        type: 'object', additionalProperties: false,
        required: ['region', 'expected', 'actual', 'severity'],
        properties: {
          region: { type: 'string' },
          expected: { type: 'string' },
          actual: { type: 'string' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
  },
}

const SYNTH = {
  type: 'object', additionalProperties: false,
  required: ['overall', 'top_fixes', 'summary'],
  properties: {
    overall: { type: 'string', enum: ['PASS', 'FAIL'] },
    top_fixes: {
      type: 'array', items: {
        type: 'object', additionalProperties: false,
        required: ['fix', 'region', 'severity'],
        properties: {
          fix: { type: 'string' },
          region: { type: 'string' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
    summary: { type: 'string' },
  },
}

phase('Critique')
const results = await pipeline(
  screens,
  (s) => parallel(LENSES.map((l) => () =>
    agent(
      `${RUBRIC(s)}\n\nYOUR ASSIGNED LENS — ${l.key}:\n${l.focus}\n\nStay in your lens but note an obvious out-of-lens defect. Return your structured verdict. "looks fine" without per-region evidence from BOTH images is invalid.`,
      { label: `${s.screen}:${l.key}`, phase: 'Critique', schema: VERDICT },
    ),
  )).then((vs) => ({ s, verdicts: vs.filter(Boolean) })),
  ({ s, verdicts }) => agent(
    `Synthesize ${verdicts.length} independent VISUAL critic verdicts for the Silencer screen "${s.screen}" (numeric pixdiff vs golden = ${s.pixdiff}%).\n\nCritic JSON:\n${JSON.stringify(verdicts)}\n\nRules: overall = PASS ONLY IF no critic returned FAIL AND zero high-severity discrepancies. Dedupe overlapping discrepancies; rank by severity; produce an ACTIONABLE ordered top_fixes list phrased as concrete edits (which element, what to change, target pixel size/position/hue). If any critic flagged palette-flattening, spaced cards, oversized/modern type, undersized buttons, or missing sprite backdrops, overall MUST be FAIL.`,
    { label: `synth:${s.screen}`, phase: 'Report', schema: SYNTH },
  ).then((synth) => ({ screen: s.screen, pixdiff: s.pixdiff, overall: synth && synth.overall, top_fixes: (synth && synth.top_fixes) || [], summary: synth && synth.summary })),
)

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
          location: { type: 'string' },
          detail: { type: 'string' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
  },
}
const CODE_PREAMBLE = `You are a 2026 senior reviewer enforcing clean, idiomatic component code (shadcn/React sensibility ported to this C++/cppx UI). Inspect the working-tree diff: run \`git --no-pager diff -- ${codeFiles || '.'}\` and \`git --no-pager diff --staged -- ${codeFiles || '.'}\` with Bash. Only review ADDED/CHANGED lines, not pre-existing code. The engine-idiom golden is /Users/hv/repos/ui — when unsure whether a pattern is idiomatic cppx, read how that repo's own components do it. Deterministic smells (raw Color{} paint, >6-case switches, fat props/signatures, god views, conditional hooks) are machine-checked by clients/silencer/tools/react_architecture_guard.py — don't re-litigate those; focus on the judgement calls in your lens. Verdict NEEDS_WORK if any high/medium finding exists. ZERO tolerance for noise: every finding cites file:line or a verbatim snippet.`

const CODE_LENSES = [
  { key: 'hygiene', focus: 'HYGIENE. (1) bloat-comment: comments restating code, narrating the obvious, or padding backstory — quote each verbatim; a comment earns its place ONLY by explaining a non-obvious WHY (the owner explicitly rejects comment bloat). (2) overengineering: needless abstraction, single-use indirection, speculative flexibility, defensive handling of impossible states. (3) dead-code.' },
  { key: 'composition', focus: 'COMPOSITION. Screens should read top-down as a tree of named primitives, not procedural orchestration. Flag repeated structure that should be extracted into the shared primitives (clients/silencer/src/client/ui/components/), one-off inline styling where a variant of an existing primitive exists, and view-function sections that are really components wanting out.' },
  { key: 'state', focus: 'STATE & DATA FLOW. State in the narrowest owner; derived values computed at build, never stored-and-synced; cross-cutting state (focus, session, settings, theme) flows through providers/capability hooks (use_session/use_settings/...), not threaded through props structs (prop drilling). Hooks unconditional at the top of the view; gameplay reached only through hook intent closures. Flag any pass-through prop crossing 2+ levels untouched.' },
  { key: 'controlflow', focus: 'CONTROL FLOW. Variants/modes dispatched via data (token maps; small local variant->token switches are the documented convention) — but flag MVC-shaped god-dispatchers: functions that know every screen/mode, switch pyramids growing a case per feature, boolean-flag params forking a function into two behaviors, deep conditional nesting that should be early returns or separate components.' },
]

const codeReviews = (await parallel(CODE_LENSES.map((l) => () =>
  agent(`${CODE_PREAMBLE}\n\nYOUR ASSIGNED LENS — ${l.key}:\n${l.focus}\n\nStay in your lens but you may note an obvious out-of-lens defect.`,
    { label: `code:${l.key}`, phase: 'CodeHygiene', schema: CODE }),
))).filter(Boolean)
const code = {
  verdict: codeReviews.some((r) => r.verdict === 'NEEDS_WORK') ? 'NEEDS_WORK' : 'CLEAN',
  findings: codeReviews.flatMap((r) => r.findings),
}

return { results: results.filter(Boolean), code }
