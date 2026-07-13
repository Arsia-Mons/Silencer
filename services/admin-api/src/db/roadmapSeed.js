// Initial roadmap seed — mirrors docs/roadmap.md (#310). Inserted on first boot
// when the roadmapitems collection is empty. Editing these in the dashboard
// afterwards does not re-seed; this is a one-time bootstrap only.

export const ROADMAP_SEED = [
  // 1. Game modes — finish stubs
  { section: 'modes', title: 'Extraction mode win logic', effort: 'M', buildsOn: 'secret lifecycle (spec 07)', detail: 'Deliver N secrets and beam out survivors. extraction_mode.h IsMatchOver.' },
  { section: 'modes', title: 'Sabotage mode win logic', effort: 'M', buildsOn: 'dets + hacking', detail: 'Plant a Plasma Detonator on the enemy TechStation; defenders win on timer.' },
  { section: 'modes', title: 'Manhunt mode win logic', effort: 'M', buildsOn: 'radar', detail: 'One marked target per team; killing the target scores. Target shown on radar.' },
  { section: 'modes', title: 'Team Deathmatch win logic', effort: 'S', buildsOn: 'team frags', detail: 'First team to score limit. Trivial once IsMatchOver reads team frags.' },
  { section: 'modes', title: 'Control Points mode', effort: 'M', buildsOn: 'hacking dwell', detail: 'Capture-and-hold map terminals; score per tick held.' },
  { section: 'modes', title: 'Vertical Heist (asymmetric)', effort: 'L', buildsOn: 'secrets, jetpack, gadgets', detail: 'Infiltrators steal a secret from the lowest base and jetpack it to a rooftop drop; Wardens defend and collapse platforms.' },
  { section: 'modes', title: 'Blackout mode', effort: 'M', buildsOn: 'hacking, lighting, radar', detail: 'One team hacks terminals to kill the lights section-by-section; the other defends.' },

  // 2. Mechanics
  { section: 'mechanics', title: 'Jetpack fuel war (EMP drains fuel)', effort: 'S', buildsOn: 'EMP grenade, max_fuel', detail: 'EMP grenades drain enemy fuel; a powerup/hack refills it. Verticality becomes contested.' },
  { section: 'mechanics', title: 'Fall damage & blast knockback', effort: 'M', buildsOn: 'grenades, platforms', detail: 'Shaped/plasma blasts launch agents; pit deaths become a threat and a weapon.' },
  { section: 'mechanics', title: 'Ceiling / wall cling', effort: 'M', buildsOn: 'player FSM', detail: 'Hold position on a ceiling to drop ambushes or break a sightline.' },
  { section: 'mechanics', title: 'Secret "heat" trail on radar', effort: 'S', buildsOn: 'secret trace_time, radar', detail: 'A carried secret periodically pings its location on radar.' },
  { section: 'mechanics', title: 'Agency signature movement', effort: 'L', buildsOn: 'agencies, player FSM', detail: 'STATIC=blink, CALIBER=double-jump, LAZARUS=glide, NOXIS=wall-cling, BLACK ROSE=grapple.' },
  { section: 'mechanics', title: 'Hack-to-sabotage environment', effort: 'M', buildsOn: 'hacking, terminals', detail: 'Terminals toggle doors/turrets/lights/trapdoors map-wide.' },

  // 3. Weapons
  { section: 'weapons', title: 'Railgun (charged pierce shot)', effort: 'M', buildsOn: 'projectile model', detail: 'High shield+health damage, long cooldown, pierces. Pairs with vertical sniping lanes.' },
  { section: 'weapons', title: 'Grappling hook (traversal)', effort: 'L', buildsOn: 'projectile + player FSM', detail: 'Anchors and reels; BLACK ROSE signature.' },
  { section: 'weapons', title: 'Sticky bomb launcher', effort: 'M', buildsOn: 'grenade/det physics', detail: 'Projectile adheres to platforms/players and detonates on a timer.' },
  { section: 'weapons', title: 'Smoke grenade (SMOKE)', effort: 'M', buildsOn: 'grenades', detail: 'Blocks line of sight and guard targeting in a radius.' },
  { section: 'weapons', title: 'Decoy grenade (DECOY)', effort: 'M', buildsOn: 'deployables', detail: 'Reads as a player to guards/robots/cameras, drawing fire.' },

  // 4. NPCs
  { section: 'npcs', title: 'VIP / Hostage NPC', effort: 'M', buildsOn: 'civilian FSM', detail: 'Neutral escort target for Escort/Assassination modes.' },
  { section: 'npcs', title: 'Sentry Drone (flying patrol)', effort: 'M', buildsOn: 'guard targeting + free flight', detail: 'Ignores platforms; pressures jetpack airspace.' },
  { section: 'npcs', title: 'Counter-hacker NPC', effort: 'M', buildsOn: 'terminal states', detail: 'Government NPC that re-secures hacked terminals.' },
  { section: 'npcs', title: 'Riot Guard (shielded melee)', effort: 'M', buildsOn: 'guard BT', detail: 'Advances behind a frontal shield; flank or EMP to break.' },

  // 5. Object / entity types
  { section: 'objects', title: 'MovingPlatform (TRACK rider)', effort: 'L', buildsOn: 'platform TRACK type', detail: 'Platform that traverses a track and carries riders.' },
  { section: 'objects', title: 'Trapdoor / BlastDoor', effort: 'M', buildsOn: 'platforms, hacking', detail: 'Hack/EMP-toggled floor or gate; the hook for hack-to-sabotage.' },
  { section: 'objects', title: 'DestructibleWall', effort: 'M', buildsOn: 'shaped grenade, platforms', detail: 'Platform segment shaped charges can blow open to create shortcuts.' },
  { section: 'objects', title: 'Zipline', effort: 'M', buildsOn: 'new traversal object', detail: 'One-way silent traversal lane bypassing jetpack noise.' },
  { section: 'objects', title: 'SecretCache', effort: 'M', buildsOn: 'PickUp, secrets', detail: 'Stash point where a carried secret can be hidden and retrieved later.' },

  // 6. Items / economy / agencies
  { section: 'items', title: 'Jammer (deployable)', effort: 'M', buildsOn: 'radar powerup', detail: 'Suppresses enemy radar/minimap in a radius. Uses a free tech bit.' },
  { section: 'items', title: 'Ammo Cache (deployable)', effort: 'M', buildsOn: 'InventoryStation', detail: 'Teammates resupply from it away from base.' },
  { section: 'items', title: 'Overdrive powerup (sub-type 7)', effort: 'S', buildsOn: 'powerups', detail: 'Temporary fire-rate / move-speed boost. Time-based, always temporary.' },
  { section: 'items', title: 'Phase powerup (sub-type 8)', effort: 'S', buildsOn: 'powerups', detail: 'Brief pass-through-projectiles window. Time-based, always temporary.' },
  { section: 'items', title: 'Sixth agency: Vox (info warfare)', effort: 'L', buildsOn: 'team colors, agency tech', detail: 'Exclusive Jammer, hacking-disruption focus. Low priority.' },
];
