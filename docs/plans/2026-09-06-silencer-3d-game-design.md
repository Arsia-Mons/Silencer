# Silencer 3D — Game Design Document

**Version:** 0.1 — discussion draft, 2026-09-06

**Confirmed direction:** Reimagine Silencer as a full 3D shooter in Unreal Engine.

**Under consideration:** Lyra as the foundation.

**Proposed starting point:** Third-person, team-based espionage with jetpack combat. Camera, scope, platform, and all new tuning values await owner decisions and playtesting.

**Tracking:** [Issue #350](https://github.com/Arsia-Mons/Silencer/issues/350).

## 1. Vision

Silencer is a multiplayer espionage shooter where rival agencies infiltrate a government facility, hack intelligence, intercept secret couriers, and raid hidden bases. Agents use jetpacks, specialized weapons, and deception to bring three secrets home before their opponents.

The player fantasy is an agile secret agent who can win through information, timing, teamwork, and combat. A successful match should produce stories such as: “We distracted security, stole their secret in the transit hall, and escaped through the upper maintenance route while our teammate defended the base.”

The conversion should preserve the decisions that make Silencer distinctive while rebuilding movement, camera, combat spaces, and feedback for three dimensions. Existing sprite animation timings and pixel velocities are reference material, not 3D tuning targets.

## 2. What the existing game gives us

This assessment comes from the repository's [game specifications](../specs/README.md), especially [player mechanics](../specs/03-player-mechanics.md), [weapons](../specs/04-weapons-and-projectiles.md), [agencies](../specs/05-teams-and-agencies.md), [objectives](../specs/07-game-objectives.md), [security](../specs/08-npcs-and-security.md), and [maps](../specs/09-map-and-environment.md), with spot checks of team and player code at commit `70591be3`. It is not a completed playtest or exhaustive code audit.

One material discrepancy: the objectives spec describes trace expiry as dropping the secret, but the current [player implementation](../../clients/silencer/src/actors/player/player.cpp) clears the carried secret and calls `KillByGovt` when the timer reaches zero. The [team implementation](../../clients/silencer/src/game/actor/team.cpp) also delegates objective events to the selected game mode and reads several thresholds from data. Treat the specs as documented design history; verify live rules before claiming exact fidelity.

| Existing design | Purpose to preserve | Proposed 3D treatment |
| --- | --- | --- |
| Hack terminals to acquire files and secret information | Exposure creates resources and objective opportunities | Physical terminals in contested spaces with interruptible interactions |
| Retrieve and deliver three secrets | Objective victories and courier interception | Preserve the three-secret victory condition and physical carrying |
| Files converted to credits at base | Returning home competes with staying in the field | Keep carried files at risk; bank them to buy equipment |
| Jetpack, jump, crouch, ledge climbing | Skilled traversal and escape | Fuel-limited jetpack, jump, crouch, and mantle in layered 3D spaces |
| Separate shield and health damage | Weapon switching and distinct combat roles | Preserve blaster/laser/rocket/flamer identities; retune damage and ranges |
| Disguises, guards, robots, civilians | Infiltration and an independent security presence | Readable disguise rules and predictable security patrols |
| Discoverable, relocatable base entrances | Intelligence gathering, raids, and deception | Fixed bases first; restricted entrance relocation in a later experiment |
| Team-contributed technology and sabotage | Team planning and disruption | Shared technology selection, purchases at base, repairable sabotage |
| Five agencies | Identity and asymmetric tactics | Preserve names and themes; balance new mechanics through playtests |
| Up to six teams, four players per team | Rivalries and third-party interference | Start at two teams of four, then test three teams before considering six |

In the documented original, terminal information fills a team meter; a large terminal then prepares a secret. The intended team retrieves it and races a trace timer. Dropped secrets can be stolen and delivered by opponents. This sequence is the core objective design to carry forward.

## 3. Design pillars

1. **Information creates opportunities.** Knowing which terminal is active, where a courier is heading, or where a base entrance sits changes the team's plan.
2. **Movement has a cost.** Jetpacks create powerful routes but consume fuel, expose silhouettes, and announce movement through sound.
3. **Combat serves the operation.** Winning a gunfight opens a hack, protects a courier, or enables a raid. Kills do not directly win the primary mode.
4. **Deception has counterplay.** Disguise and surveillance create uncertainty while giving attentive opponents understandable clues.
5. **Every advantage can be contested.** Valuable terminals have multiple approaches; sabotage is repairable; respawn areas resist trapping.

## 4. Recommended product scope

| Decision | Initial proposal | Status |
| --- | --- | --- |
| Perspective | Third-person shoulder camera with aimed view | Open — full 3D shooter is confirmed, perspective is not |
| Platform | PC first; keyboard/mouse and controller | Open |
| Primary mode | Secret War: race to deliver three secrets | Proposed preservation of the original objective |
| Match size | 4v4; use 1v1 and 2v2 for initial engineering tests | Proposed prototype scope |
| Match duration | Aim for 15–20 minutes; provisional 20-minute limit | Tuning hypothesis |
| Multiplayer | Authoritative dedicated server | Recommended |
| Art direction | Stylized industrial science fiction with agency silhouettes | Open |
| Progression | Equal combat access in prototype; cosmetics/mastery later | Proposed change from original persistent stat upgrades |
| Business model | Undecided; no monetization systems in prototype | Open |

Third-person is recommended because it exposes jetpack motion, agency silhouettes, disguises, and carried equipment. It also creates corner-peeking advantages that must be tested in hacking and ambush spaces. First-person is a valid alternative if weapon immersion and restricted awareness matter more. Do not support both competitive perspectives initially; decide before producing final animations and maps.

## 5. Primary mode: Secret War

### Match flow

1. Players join an agency team and choose equipment/technology contributions.
2. Teams deploy at their bases and move into a shared government facility.
3. Agents hack terminals to carry files and advance their team's intelligence meter.
4. A full meter initiates secret preparation at an eligible large terminal.
5. The team retrieves the secret; opponents can ambush the courier and steal a drop.
6. The courier returns to their own base and deposits the secret.
7. First team to bank three secrets wins. The match summary emphasizes deliveries, steals, hacking, and team support alongside combat.

### Proposed playable rules

These rules make the first prototype testable. Where they simplify or extend the original, they are new proposals.

- **Hacking:** Hold interact while near and facing a terminal. Movement, firing, jetpack activation, death, or loss of interaction range cancels hacking. Only one hacker uses a terminal at a time. Earned files and team intelligence remain earned; unused terminal capacity stays available. Terminal recovery timing is a tuning variable.
- **Intelligence:** Shared by the team and displayed as 0–100%. One pending or carried team-origin secret at a time for the first prototype. Meter generation pauses until that secret is deposited or destroyed; theft does not create a second copy.
- **Preparation:** Select an available large terminal from authored eligible locations. Provisional preparation time: 30 seconds. The owning team gets its location immediately. Nearby opponents can hear/see activity; they do not receive a global exact marker.
- **Retrieval:** The originating team can interact to collect the prepared secret. Other teams must intercept it after pickup and force a drop. This preserves the distinction between retrieval and theft.
- **Trace:** Provisional 120-second timer begins when the secret becomes collectible. It continues while carried or dropped. Expiry destroys the prototype secret and reopens its originating team's intelligence cycle, without killing its carrier. This is a proposed simplification; see the documented-versus-implemented expiry discrepancy above.
- **Carrying:** One secret per agent. Keep normal weapon access and baseline movement during the first test. Carrier and allies see the remaining timer. Opponents identify a visible courier by a distinctive carried device; no constant global carrier tracking initially.
- **Death/disconnect:** Drop carried files and the secret at the last valid playable location. Server ownership changes atomically so simultaneous pickups cannot duplicate it. Out-of-bounds drops move to the nearest authored recovery point without resetting the trace timer.
- **Theft:** Any team may collect a dropped secret and deliver it to their own base. Score follows the delivering team; the original team ID remains for theft statistics.
- **Delivery:** Hold interact for a provisional three seconds at your base return station. Damage, departure, or death interrupts deposit. A completed deposit consumes the secret, awards one point, and grants every teammate a fixed credit reward. Already banked secrets cannot be stolen in the prototype.
- **Respawn:** Provisional eight-second delay, followed by spawn in a protected base room with multiple exits. Protection ends on leaving that room; objectives and attack positions sit outside it. No revive system in the first test.
- **Time limit:** At 20 minutes, most banked secrets wins. If tied, allow a maximum three-minute overtime in which the first delivery wins; if nobody scores, declare a draw. These are new pacing rules requiring playtests.

### What a minute of play feels like

An agent hacks while a teammate watches an upper walkway. An enemy jetpack announces an approach. The hacker leaves early with partial files, the teammate strips the attacker's shield, and both retreat through a service route. At base, those files fund a rocket resupply. Meanwhile, their team's secret begins preparing in the archive, changing the next objective from resource collection to escort and interception.

## 6. Movement, camera, and combat

### Movement

Begin with run, crouch, jump, mantle, and a fuel-limited jetpack. Use the jetpack for crossing gaps, changing floors, and brief combat repositioning. Refuel on grounded downtime. Prototype approximately three seconds of continuous thrust, then tune around route choice and exposure rather than the original tick values.

Keep a ground route to every mandatory objective. Jetpack routes should save time or create a flank while exposing the agent. Airborne firing is supported with an accuracy tradeoff. Defer sliding, wall-running, grappling, prone movement, and elaborate traversal chains until the core movement is proven.

Camera acceptance checks: no seeing through walls, no muzzle shots through blocked cover, readable targets above and below, stable aiming during thrust, and usable terminal interactions in narrow spaces. If third-person is chosen, test shoulder switching and corner visibility with opposing players.

### Combat model

Use shield plus health and recognizable shield-break feedback. Test a baseline of 100 health and 100 shield. Initially restore both at base rather than adding passive regeneration; this keeps retreat and resupply meaningful. Target roughly 1.5–2.5 seconds for a competent close-to-medium-range weapon combination against a fully protected target, then adjust from measured encounters.

| Weapon | Role | Prototype treatment |
| --- | --- | --- |
| Blaster | Reliable health-finishing sidearm | Unlimited reserve, moderate cadence, weak against shields |
| Laser | Shield removal and precision | Limited ammunition, clear beam/tracer, low health damage |
| Rocket | Splash pressure and punishing predictable movement | Visible projectile, scarce ammunition, self-damage |
| Flamer | Close-range shield bypass and denial | Preserve role in design; implement after the first combat test |

Prototype blaster and laser first; add the rocket for the vertical slice. Use projectile rockets and evaluate hitscan for the laser as a deliberate 3D adaptation. Set range, cadence, projectile speed, aim assistance, and damage through playtesting. Do not copy Lyra weapon balance unchanged. Friendly fire starts disabled; explosives cannot deal damage through solid walls.

## 7. Espionage, security, and equipment

### Disguise

Preserve civilian impersonation as a distinctive feature, but add it after the secret loop works. While disguised, conceal the agent's enemy nameplate and imitate a civilian silhouette and walking behavior. Firing, jetpacking, hacking, or taking damage breaks the disguise. Friendly teammates retain identification. A carried secret prevents disguise in the first experiment.

Avoid making civilians and disguised players trivially distinguishable by different animation quality, mandatory outlines, or inconsistent collision. Start with one civilian body and a small shared animation set. Test whether opponents can infer an infiltrator from behavior without memorizing an arbitrary visual tell.

### Government security

The vertical slice uses one guard type: patrol, detect, investigate, engage, search, return. Gunfire and jetpacks provide investigation cues; line of sight determines confirmed detection. Show readable alert feedback. Guards should delay or expose agents without dominating PvP. Begin with a fixed small population; tune count after server and playtest measurements.

Robots, civilian conversion, and virus-controlled security follow later. Existing behavior-tree JSON provides design reference; it is not an Unreal AI asset that can be imported unchanged.

### Equipment and sabotage

Begin with one EMP grenade to create a simple shield counterplay test. The next additions should be a surveillance camera and a repairable tech-station virus, since both reinforce the espionage theme. Later consider health packs, deployable cannons, remote explosives, security passes, and agency exclusives.

Defer neutron bombs, invisibility, and unrestricted base relocation. They introduce large readability and balance changes before the new maps and camera are understood.

## 8. Agencies, economy, and progression

Keep Noxis, Lazarus, Caliber, Static, and Black Rose as the identity framework. Their documented themes are endurance, resurrection, contacts/security access, hacking/sabotage, and shield/poison respectively. Exact original stat bonuses are not automatically retained.

Use identical agents with different team identification in the first prototype. Then test two agencies with one meaningful specialty each. Expand to all five only when those differences improve decisions without overwhelming new players. Agency is a team affiliation; roles such as hacker, escort, infiltrator, and defender emerge from loadout and play rather than mandatory hero classes.

Files are unbanked resources dropped on death. Credits are personal, earned by depositing files, retained on death, and spent at the base. Technologies are team permissions contributed by loadouts. Distinguish these three concepts in the interface. Start with a small fixed shop and equal starting credits; avoid random loot and permanent equipment loss.

Permanent account upgrades in the original can increase health, shields, fuel, hacking, and tech capacity. Recommendation for the new competitive game: provide equal gameplay access and use long-term mastery/cosmetics for progression. This is a design change requiring owner agreement. Account migration is a separate decision; do not promise automatic transfer of old combat upgrades.

## 9. First map: government relay facility

Build one compact graybox with an archive, transit hall, maintenance route, and two team bases. The central space has two useful combat elevations; avoid stacking so many floors that locating enemies becomes confusing. Use recognizable landmarks, floor numbers, and distinct lighting for orientation.

Give each major terminal at least two approaches, cover with interruption opportunities, and an escape route. Include at least two eligible large terminals so secret preparation does not always produce the same fight. Start with four small terminals and adjust from travel and contention measurements.

Target 20–30 seconds from base to a useful terminal and 30–45 seconds from a secret terminal to base along a safe route. Those are measurement goals, not fixed map dimensions. Provide a faster exposed jetpack route and a slower sheltered ground route. Avoid long open sightlines that let one elevated position dominate every objective.

Bases begin at fixed, clearly marked positions. The return station, shop, and sabotagable station are in raidable areas; the respawn room is separate. This sacrifices hidden entrance play temporarily to make objective routing testable. Later test relocating a portal among a few authored sockets, with placement exclusions around objectives and enemy spawns.

## 10. Presentation and player experience

Recommended visual direction: industrial science fiction, covert agency equipment, readable armor silhouettes, and strong terminal/security lighting. Use original art as mood and identity reference. New characters, weapons, animation, environments, and effects require 3D production; sprites and tile maps are not finished Unreal assets.

The HUD prioritizes team secret score, current operation, carried files/secret, health/shield, ammunition, fuel, and selected gadget. Show an objective's floor and distance. Only show opponent information earned through visibility or a defined detection mechanic. A full-map always-on enemy radar would undermine infiltration.

Use sound to distinguish jetpack thrust, shield break, active hacking, secret readiness, alarms, and deposit completion. Pair important sounds with visual cues. Provide remappable controls, text scaling, subtitles/captions for gameplay cues, color-independent team identification, and adjustable camera shake. Spectators must not leak hidden opposing-team information to active teammates.

First-session onboarding teaches one short sequence: move and jetpack, strip a shield and finish, hack, bank files, retrieve a secret, and deliver. Post-match results should explain objective contributions so support players understand their value.

## 11. Unreal and Lyra recommendation

**Recommendation: evaluate a Lyra-based prototype before committing the full production project.** Lyra supplies a multiplayer shooter example and modular gameplay foundation, but it does not supply Silencer's espionage mode. Epic describes it as a sample and learning resource. [Epic: Lyra overview](https://dev.epicgames.com/documentation/unreal-engine/lyra-sample-game-in-unreal-engine?application_version=5.8).

| Area | Reuse/evaluate | Silencer work |
| --- | --- | --- |
| Shooter foundation | Lyra pawn, camera, input, animation, weapon examples | Movement feel, jetpack prediction, weapon roles, chosen perspective |
| Abilities | Unreal Gameplay Ability System as used by Lyra | Fuel, shields, hacking restrictions, disguise, EMP and sabotage |
| Items | Lyra inventory/equipment concepts | File carrying, credits, shop and team technology rules |
| Interaction | Lyra interaction example | Terminal capacity, hacking interruption, pickup and deposit validation |
| Match flow | Lyra Experiences and game feature structure | Secret War lifecycle, team intelligence, scoring, overtime |
| Networking | Unreal replication and dedicated-server approach | Hidden information, authoritative objectives, reconnect/drop behavior |
| Security | Unreal AI tooling | Government perception, patrols, disguise recognition, allegiance changes |

Lyra uses Gameplay Ability System abilities, effects, and attributes for gameplay; its equipment can grant abilities, and its interaction example uses a gameplay ability with an interaction interface. These are useful implementation starting points, not completed espionage mechanics. [Abilities](https://dev.epicgames.com/documentation/unreal-engine/abilities-in-lyra-in-unreal-engine), [equipment](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-inventory-and-equipment-in-unreal-engine), [interaction](https://dev.epicgames.com/documentation/unreal-engine/lyra-sample-game-interaction-system-in-unreal-engine).

Keep the new mode in a focused Silencer game feature and minimize changes to Lyra core. Use C++ for authority, replicated state, and movement; use Blueprints/data assets for interaction presentation, content assembly, and tuning. A jetpack ability alone does not solve predicted movement: explicitly prototype its movement-component integration and server correction behavior.

The repository already calls another system “GAS.” Unreal GAS means Gameplay Ability System; do not assume the existing `shared/gas-validation` schemas are compatible or reuse its assets automatically.

Pin an engine release and matching Lyra sample after verifying local toolchain and server packaging. Epic notes that C++ Lyra projects require manual work across major engine upgrades. Treat upgrades as scheduled work rather than an automatic dependency update. [Epic: upgrading Lyra](https://dev.epicgames.com/documentation/en-us/unreal-engine/upgrading-the-lyra-starter-game-to-the-latest-engine-release-in-unreal-engine).

If Lyra's framework makes a two-client objective prototype harder to extend than a small Unreal project, reassess before creating content. This is the decision gate: two remote clients can hack, fight, carry/drop/steal a secret, and score reliably on a dedicated server, with code the developer understands and can change.

## 12. Existing services and migration boundaries

The SDL3 game runtime, renderer, collision, snapshot protocol, and sprite animation pipeline need new Unreal implementations. Preserve the existing game as a behavioral reference during development. Rebuild maps around routes and sightlines; do not plan a literal tile-to-mesh conversion as the finished level design.

The Go lobby, accounts, admin API, and dashboard may remain useful as service infrastructure. Their current client protocol and game-process lifecycle need explicit integration with an Unreal client/server; Lyra cannot join the existing lobby or speak the current gameplay protocol automatically. Start the prototype with direct server connection, then decide whether to retain the Go control plane or adopt a different session service. Epic also documents Lyra integration with EOS; that is an option to evaluate, not a requirement of this design. [Epic: Lyra with EOS](https://dev.epicgames.com/documentation/unreal-engine/using-lyra-with-epic-online-services-in-unreal-engine).

Production acceptance must include a real login → session allocation → Unreal server join → complete match → results recorded flow if the existing services are retained. Client-side UI success alone is insufficient.

Use a dedicated server for competitive play. Epic's documented Lyra server tutorial uses a source-built engine and a C++ multiplayer project; include that build/toolchain work in the initial technical investigation. [Epic: dedicated servers](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine).

## 13. Prototype and vertical slice

Schedule follows team size, Unreal experience, available assets, and target hardware. Use completion gates before assigning calendar estimates.

| Stage | Deliverable | Exit condition |
| --- | --- | --- |
| A — Foundation decision | Matching Unreal/Lyra build, selected camera, graybox movement, two remote clients | Packaged clients join a dedicated server; movement and shooting work under latency |
| B — Espionage loop | One simple map, two bases, blaster/laser, terminals, files, credits, one-secret test victory | Two teams hack, retrieve, drop, steal, and deposit; match ends consistently |
| C — Playable Secret War | 4v4, three-secret scoring, jetpack, rocket, EMP, trace and overtime | Repeated complete matches with understandable objectives and viable attack/defense routes |
| D — Identity slice | One polished map section, one guard, civilian disguise experiment, two agency specialties | Players use both information and combat to win; presentation supports those decisions |
| E — Service integration | Chosen account/session path and saved results | Real end-to-end join-to-results test through all retained services |

The first playable does not need all agencies, six teams, all weapons, full account progression, a map editor, ranked matchmaking, multiple modes, or a large map collection. Add these only after the core mode proves enjoyable and sustainable to produce.

## 14. Validation and risks

Measure match duration, time to first secret, deliveries versus steals, hack interruption rate, base exit deaths, time spent traveling, weapon use, and where couriers die. Combine those numbers with player explanations; a short match can still be confusing or one-sided.

Required multiplayer checks include simultaneous secret pickup, interruption at the deposit deadline, carrier death/disconnect, trace expiry, score replication, late join, and match reset. Test movement and objective actions at representative latency, including roughly 100 ms round-trip and light packet loss. Ensure enemy UI and replication do not reveal information the design intends to hide.

Performance proposal: aim for 60 FPS on an agreed PC baseline with eight players and the slice's AI population. Select actual hardware and server tick/performance budgets during Stage A; neither is validated by this document.

| Risk | Early test or response |
| --- | --- |
| The game becomes mostly team deathmatch | Observe whether terminal control and secret routes actually decide outcomes |
| Third-person corner peeking overwhelms stealth | Test opposing players around hacks, doors, and vertical cover before locking camera |
| Jetpacks bypass every choke or cause network corrections | Graybox fuel/route tests and dedicated-server latency tests |
| Bases become spawn traps | Separate spawn protection from raidable stations; provide multiple exits |
| Economic advantage compounds too quickly | Measure purchase advantage and adjust rewards, equipment costs, and baseline weapons |
| Disguise is either useless or unreadable | Test civilian behavior, identification, and action-breaking rules with unfamiliar players |
| Agency asymmetry obscures core balance | Establish a symmetric baseline before adding specialties |
| Art scope exceeds capacity | One environment kit and one agent rig first; establish production cost before expanding |
| Lyra integration absorbs development time | Require the Stage B objective gate before committing to content production |

## 15. Decisions for the owner

1. Third-person or first-person? Full 3D shooter is already confirmed.
2. Preserve espionage as the primary loop, or shift toward combat with lighter objectives?
3. Solo development or a team? What Unreal experience, budget, and approximate timeline are available?
4. PC only initially, and what minimum hardware should be supported?
5. Stylized industrial science fiction, realistic covert operations, or another visual reference?
6. Is 4v4 an acceptable first target, or is multi-agency free-for-all essential immediately?
7. Keep persistent combat upgrades from the original, or adopt equal competitive access?
8. Must existing accounts, the Go lobby, and self-hosting work in the first public build?

Once the first four are answered, revise this draft into an agreed direction and a scoped prototype backlog. This document proposes a game; it does not claim an Unreal prototype has been built or validated.
