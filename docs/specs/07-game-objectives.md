# 07 — Game Objectives

## Win Condition

A team wins the game by delivering **3 secrets** to their **SecretReturn**
point inside their base. When `team.secrets >= 3`, the team's ID is stored as
`winning_team_id`. All members of the winning team are un-deployed
(beam-out); all other players are killed by the government.

## Secret Lifecycle

Secrets go through a multi-step process:

### 1. Locating a Secret (Insider Info / Secret Progress)

Each team tracks a `secret_progress` counter (0–180). Progress is gained by:

- **Buying Insider Info** — adds **+20** to `secret_progress` (costs 500
  credits). Cannot exceed 160 via Insider Info alone ("Insiders can never
  provide Location of Top-Secret").
- **Hacking terminals** — terminals with `secret_info > 0` contribute progress
  proportional to hack completion. The effective hacking rate is scaled by
  `1 + hacking_bonus + hacking_powerup_bonus`.

When `secret_progress >= 180`:

1. The server picks a random available **big terminal** (one that is not
   already beaming or has a secret ready).
2. That terminal begins **SECRETBEAMING** — a timed process that takes
   `beaming_seconds` to complete (set per terminal, typically 30–60s).
3. The team's `beaming_terminal_id` is set.

### 2. Terminal Beaming

- The selected terminal enters the **BEAMING** state.
- Every 24 ticks (1 second), `beaming_count` increments.
- When `beaming_count >= beaming_seconds`, the terminal transitions to
  **READY** and eventually to **SECRETREADY**.
- A **trace timer** is set based on how many secrets the team has already
  delivered:

| Secrets Delivered | Trace Time (seconds) |
|-------------------|---------------------|
| 0 | 150 |
| 1 | 120 |
| 2 | 90 |

If the trace timer expires before the secret is picked up, the terminal
resets and the team's `beaming_terminal_id` is cleared (the secret is lost).

### 3. Picking Up the Secret

When a player walks over a terminal in the **SECRETREADY** state AND:
- The player's team matches the terminal's beaming team.
- The player is not already carrying a secret.

The player picks up the secret:
- `has_secret` = true
- `secret_team_id` = the team that owns this secret
- `trace_time` transfers from the terminal to the player
- The terminal resets to INACTIVE.

### 4. Carrying the Secret

While carrying a secret:
- If the player has a `trace_time > 0`, it decrements every 24 ticks.
- If `trace_time` reaches 0, the secret is dropped as a PickUp.
- If the player dies, the secret is dropped.
- Dropped secrets can be picked up by any player (including enemies — this
  counts as "stealing" a secret).

### 5. Delivering the Secret

The player must bring the secret to their team's **SecretReturn** object
inside their own base. On contact:
- `team.secret_delivered` is set (triggers score increment).
- `team.secrets` increments by 1.
- All team members receive **1,000 credits**.
- A screen-flash effect plays.

If the secret was stolen (original `secret_team_id` ≠ delivering team's ID),
it still counts and is tracked in stats as a stolen secret.

## Terminal Hacking

Terminals are interactive objects placed throughout the map. They cycle through
states:

| State | Description |
|-------|-------------|
| INACTIVE | Idle; regenerating (animation loop) |
| BEAMING | Counting down to become ready (periodic) |
| READY | Available for hacking; emits ambient sound |
| HACKING | A player is currently hacking |
| HACKERGONE | Hacker left before completing; slowly resets |
| SECRETBEAMING | Beaming a secret for a specific team |
| SECRETREADY | Secret is available for pickup |

### Hack Mechanics

- A player enters HACKING state by pressing `activate` near a READY terminal.
- Each tick the player remains hacking, the terminal's `state_i` increments.
- Progress is displayed as a percentage: `(state_i / juice) × 100`.
- `juice` is the terminal's total hack capacity.
- As the hack progresses, files and secret-info are extracted proportionally:
  - Files extracted = `(percent / 100) × terminal.files`
  - Secret progress = `(percent / 100) × terminal.secret_info × (1 + hacking_bonus + hacking_powerup_bonus)`
- When `state_i >= juice`, the terminal resets to INACTIVE.
- If the hacker walks away mid-hack, the terminal enters HACKERGONE and slowly
  drains remaining juice.

### Terminal Sizes

Terminals come in two sizes:
- **Small** — yields files and minor secret info.
- **Big** — additionally used for the secret beaming process.

## Files & Credits

Files are downloaded from terminals and converted to credits at a
**CreditMachine** inside the player's base.

- Max files per player: **2,800**.
- Credit conversion: `credits = files × (1 + credits_bonus)`.
- Files are dropped on death and can be picked up by anyone.
