'use client';
import Link from 'next/link';
import { useState } from 'react';
import type { ReactNode } from 'react';

const SECTIONS = [
  { id: 'overview',   icon: '◈', title: 'Overview' },
  { id: 'getting-in', icon: '◉', title: 'Getting Into The Game' },
  { id: 'movement',   icon: '◎', title: 'Movement' },
  { id: 'hud',        icon: '◧', title: 'The HUD' },
  { id: 'base',       icon: '⬡', title: 'Your Base' },
  { id: 'hacking',    icon: '◑', title: 'Hacking' },
  { id: 'weapons',    icon: '◈', title: 'Weapons' },
  { id: 'tech',       icon: '◎', title: 'Tech & Inventory' },
  { id: 'agencies',   icon: '⬡', title: 'Agencies' },
  { id: 'tricks',     icon: '◉', title: 'Disguises & Tricks' },
  { id: 'teamwork',   icon: '◑', title: 'Teamwork & Chat' },
];

function Section({ id, icon, title, children }: { id: string; icon: string; title: string; children: ReactNode }) {
  return (
    <section id={id} className="bg-game-bgCard border border-game-border rounded p-6 mb-6 scroll-mt-6">
      <h2 className="text-game-primary font-mono text-base tracking-widest mb-4 border-b border-game-border/40 pb-2">
        {icon} {title}
      </h2>
      <div className="text-game-text font-mono text-xs leading-relaxed space-y-3">
        {children}
      </div>
    </section>
  );
}

function P({ children }: { children: ReactNode }) {
  return <p className="text-game-textDim leading-relaxed">{children}</p>;
}

function Key({ children }: { children: ReactNode }) {
  return (
    <kbd className="inline-block px-1.5 py-0.5 text-xs font-mono bg-game-dark border border-game-border rounded text-game-primary mx-0.5">
      {children}
    </kbd>
  );
}

function Note({ children }: { children: ReactNode }) {
  return (
    <div className="border-l-2 border-game-warning pl-3 text-game-warning text-xs font-mono">
      ⚠ {children}
    </div>
  );
}

function ScreenShot({ src, alt, className = '' }: { src: string; alt: string; className?: string }) {
  return (
    <img
      src={src}
      alt={alt}
      className={`border border-game-border/40 rounded bg-black ${className}`}
    />
  );
}

function ItemRow({ name, cost, desc, agency, icon }: { name: string; cost?: number | null; desc: string; agency?: string; icon?: string }) {
  return (
    <tr className="border-b border-game-border/30 hover:bg-game-bgHover">
      <td className="py-1.5 pr-2 w-8">
        {icon && <img src={`/silencer/${icon}`} alt={name} className="w-7 h-7 bg-black border border-game-border/30 rounded" />}
      </td>
      <td className="py-1.5 pr-3 text-game-primary whitespace-nowrap">{name}</td>
      <td className="py-1.5 pr-3 text-right text-game-text whitespace-nowrap">{cost ?? '—'}</td>
      <td className="py-1.5 text-game-textDim">{desc}{agency && <span className="ml-2 text-game-warning text-xs">[{agency} only]</span>}</td>
    </tr>
  );
}

function AgencyCard({ name, color, bonus, img }: { name: string; color: string; bonus: string; img?: string }) {
  return (
    <div className={`border rounded p-3 ${color}`}>
      {img && (
        <img src={img} alt={name} className="h-8 mb-2 border border-game-border/30 rounded bg-black" />
      )}
      <div className="font-mono text-sm font-bold mb-1">{name}</div>
      <div className="text-xs text-game-textDim leading-relaxed">{bonus}</div>
    </div>
  );
}

export default function HowToPage() {
  const [navOpen, setNavOpen] = useState(false);

  return (
    <div className="min-h-screen">
      {/* Top bar */}
      <header className="flex items-center justify-between px-6 py-4 border-b border-game-border bg-game-bgCard sticky top-0 z-10">
        <div className="flex items-center gap-4">
          <img src="/logo.png" alt="Silencer" className="h-10 w-auto" />
          <div>
            <div className="text-game-primary text-sm font-mono font-bold tracking-widest">HOW TO PLAY</div>
            <div className="text-game-muted text-xs font-mono">AGENT FIELD MANUAL</div>
          </div>
        </div>
        <div className="flex items-center gap-3">
          <button
            onClick={() => setNavOpen(o => !o)}
            className="lg:hidden px-3 py-1.5 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary">
            ☰ SECTIONS
          </button>
          <Link href="/me"
            className="px-3 py-1.5 text-xs font-mono text-game-textDim border border-game-border rounded hover:border-game-primary hover:text-game-primary transition-colors">
            ← MY PROFILE
          </Link>
        </div>
      </header>

      <div className="max-w-6xl mx-auto flex gap-6 p-6">
        {/* Sidebar nav */}
        <aside className={`${navOpen ? 'block' : 'hidden'} lg:block w-48 shrink-0`}>
          <nav className="sticky top-24 bg-game-bgCard border border-game-border rounded p-3 space-y-1">
            <div className="text-game-muted text-xs font-mono tracking-widest mb-2 px-2">SECTIONS</div>
            {SECTIONS.map(s => (
              <a key={s.id} href={`#${s.id}`}
                onClick={() => setNavOpen(false)}
                className="block px-2 py-1.5 text-xs font-mono text-game-textDim hover:text-game-primary hover:bg-game-dark rounded transition-colors">
                {s.icon} {s.title}
              </a>
            ))}
          </nav>
        </aside>

        {/* Content */}
        <main className="flex-1 min-w-0">

          <Section id="overview" icon="◈" title="Overview">
            <P>SILENCER is a top-secret espionage action game set in a near-future Mars colony. You play as an agent working for one of five rival intelligence agencies — NOXIS, LAZARUS, CALIBER, STATIC, or BLACKROSE — each with unique abilities and playstyles.</P>
            <P>The goal of each mission is simple: collect 3 secrets before the other teams do. Secrets are located by hacking data terminals scattered around the map. Once you have gathered enough intelligence, a secret location is revealed — go pick it up and return it to the secrets terminal in your base.</P>
            <P>Between hacking runs you will earn credits, buy tech gear, defend your base, and fight off rival agents. Balance offense, defense, and intelligence gathering — no single strategy wins every time.</P>
          </Section>

          <Section id="getting-in" icon="◉" title="Getting Into The Game">
            <P>Launch SILENCER and log in with your callsign and password. After logging in you will land in the lobby — a list of open games you can join, plus a chat area to coordinate with other players.</P>
            <div className="space-y-2">
              <P>Joining a game: click a conflict in the list, then click JOIN CONFLICT. You will enter the game lobby where you wait for everyone to ready up.</P>
              <P>Game lobby: players from the same agency are on the same team by default. Use CHANGE TEAMS to split. Use the TECH button to assign your inventory slots before the game starts.</P>
              <P>Starting: click READY. Once all players are ready, the host starts the game.</P>
            </div>
            <Note>Play the tutorial missions first if this is your first time — the game has mechanics that are not obvious until you have seen them in action.</Note>
          </Section>

          <Section id="movement" icon="◎" title="Movement">
            <P>Your agent has three movement modes:</P>
            <div className="space-y-3">
              <div>
                <div className="text-game-primary mb-1">On Foot</div>
                <P>Move left/right with <Key>Numpad 4</Key> / <Key>Numpad 6</Key>. Jump with <Key>Numpad 8</Key>. Duck (hold) with <Key>Numpad 2</Key>.</P>
              </div>
              <div>
                <div className="text-game-primary mb-1">Jetpack</div>
                <P>Hold <Key>Shift</Key> to thrust upward. Hold <Key>Shift</Key> + direction to arc diagonally. Tap rapidly to traverse sideways without gaining altitude. Watch your fuel gauge — once empty it must fully recharge before you can use it again.</P>
              </div>
              <div>
                <div className="text-game-primary mb-1">Tuck &amp; Roll</div>
                <P>Duck with <Key>Numpad 2</Key> then press a direction to roll. Slower than running but harder to hit.</P>
              </div>
            </div>
            <div className="border-t border-game-border/40 pt-3 mt-3">
              <div className="text-game-primary mb-2">Advanced</div>
              <P>Ladders: walk in front of one and press <Key>Numpad 8</Key> to climb up, <Key>Numpad 2</Key> to climb down. Press <Key>Space</Key> while on a ladder to drop off — you fall faster than you climb.</P>
              <P>Ledge mantle: jump and hold the jump key while airborne near a ledge corner — your agent will automatically pull himself up.</P>
              <P>Falling: you take no damage from falls, so drop freely instead of climbing down.</P>
            </div>
          </Section>

          <Section id="hud" icon="◧" title="The HUD">
            <P>The HUD wraps around the play area. Key elements, clockwise:</P>
            <a href="/silencer/hud.jpg" target="_blank" rel="noopener noreferrer" className="block mt-2 mb-4">
              <ScreenShot src="/silencer/hud.jpg" alt="Full HUD overview" className="max-w-sm w-full opacity-90 hover:opacity-100 transition-opacity" />
              <span className="text-game-muted text-xs mt-1 block">↑ Full screen overview (click to enlarge)</span>
            </a>
            <div className="space-y-4 mt-2">
              {[
                { name: 'Agent List', desc: 'Life-sign monitors for all players. White = carrying a secret. Flat red = dead. Three dots per agent track secrets collected. First team to 3 wins.', img: '/silencer/agentlist_closeup.gif' },
                { name: 'Inventory', desc: 'Your carried items with counts. Brightest icon = currently selected item.', img: '/silencer/inventory_closeup.gif' },
                { name: 'Files Bar', desc: 'How full you are with hacked data. More files = bigger payout when you hit the regenerator in your base.', img: '/silencer/filesbar_closeup.gif' },
                { name: 'Credit Balance', desc: 'Credits available to spend on ammo and tech items.', img: '/silencer/credits_closeup.gif' },
                { name: 'Shield Level', desc: 'Your energy shield. Once depleted your body takes raw damage — keep it topped up.', img: '/silencer/shieldlevel_closeup.gif' },
                { name: 'Fuel Gauge', desc: 'Jetpack fuel. Drains continuously while used; must fully recharge before reuse. LOW FUEL light = not ready.', img: '/silencer/fuelgauge_closeup.gif' },
                { name: 'Mini-map', desc: 'Your most powerful tool. Shows: your position, enemies (red dots), enemy bases (colored rectangles), hackable terminals (white crosses — large cross = large terminal worth 2-4x data), secrets (blue = yours, red = theirs, flashing circle = being carried), and firefights (orange blips).', img: '/silencer/minimap_closeup.gif' },
                { name: 'Health Meter', desc: 'Your HP. Hits 0 = you die and respawn at your base.', img: '/silencer/healthmeter_closeup.gif' },
                { name: 'Weapon Area', desc: 'Current weapon icon and remaining ammo.', img: '/silencer/weapsinfo_closeup.gif' },
                { name: 'Information List', desc: 'Your progress toward locating the next secret. Green = done, flashing = in progress.', img: '/silencer/infolist_closeup.gif' },
              ].map(({ name, desc, img }) => (
                <div key={name} className="flex gap-4 items-start">
                  {img && <ScreenShot src={img} alt={name} className="shrink-0 max-h-16 w-auto" />}
                  <div>
                    <span className="text-game-primary block mb-0.5">{name}</span>
                    <span className="text-game-textDim">{desc}</span>
                  </div>
                </div>
              ))}
            </div>
          </Section>

          <Section id="base" icon="⬡" title="Your Base">
            <P>Your base is your respawn point, bank, and store. Deploy it at the start of the game by pressing <Key>Enter</Key>. Choose the location carefully:</P>
            <ul className="list-disc list-inside space-y-1 text-game-textDim ml-2">
              <li>Close enough to hackable terminals to minimize travel time</li>
              <li>Not behind government defenses or heavy security</li>
              <li>Not easily blockaded or infiltrated by enemies</li>
              <li>Close enough to deliver secrets quickly</li>
            </ul>
            <P>To enter your base, walk in front of it and press <Key>Space</Key>. Inside, left to right:</P>
            <div className="space-y-4 mt-2">
              {[
                { name: 'Regenerator', desc: 'Heals you as you run through it and pays credits for any files you are carrying. Also your default respawn point.', img: '/silencer/regenerator.jpg' },
                { name: 'Tech Terminal', desc: 'Buy items from your assigned tech slots here. Also repair damaged basement machinery.', img: '/silencer/techterminal.jpg' },
                { name: 'Ladder down', desc: 'Leads to the basement where your tech slot machines live. Enemies can sabotage these — a destroyed machine disables that slot until repaired.', img: '/silencer/ladder2basement.jpg' },
                { name: 'Secrets Terminal', desc: 'Deliver secrets here to score. It is at the far right — do not let enemies camp it.', img: '/silencer/secretsterminal.jpg' },
              ].map(({ name, desc, img }) => (
                <div key={name} className="flex gap-4 items-start">
                  <ScreenShot src={img} alt={name} className="shrink-0 max-h-24 w-auto" />
                  <div>
                    <span className="text-game-primary block mb-0.5">{name}</span>
                    <span className="text-game-textDim">{desc}</span>
                  </div>
                </div>
              ))}
            </div>
            <div className="mt-4">
              <div className="text-game-primary mb-1">The Basement</div>
              <div className="flex gap-4 items-start">
                <ScreenShot src="/silencer/basebasement.jpg" alt="Base basement" className="shrink-0 max-h-24 w-auto" />
                <P>At the bottom of the ladder is the lower level of your base. Each machine here controls a tech slot. If an enemy destroys one, that slot is unavailable until you repair it at the Tech Terminal.</P>
              </div>
            </div>
            <Note>Enemies can sneak into your base just as easily as you can sneak into theirs. Balance hacking runs with base defense.</Note>
          </Section>

          <Section id="hacking" icon="◑" title="Hacking">
            <P>Hacking is the core of the game. Data terminals appear as white crosses on the mini-map. Large crosses are large terminals worth 2-4x the data. A flashing cross means a large terminal is about to come online.</P>
            <P>To hack: walk up to a terminal and press <Key>Space</Key>. A countdown timer appears above it — the terminal deactivates when it hits zero, even if you stop hacking. Interruptions mean less data collected. If your agent glows orange, you are still collecting.</P>
            <div className="space-y-2">
              <P>Getting paid: run back through the regenerator in your base. The more files in your files bar, the bigger the payout.</P>
              <P>Finding secrets: once you have collected enough information from terminals, the game locks in a secret location. Go pick it up (white sphere) and carry it back to your secrets terminal. First team to return 3 secrets wins.</P>
              <P>Looting kills: if you kill an enemy who was carrying files, you can pick them up (black cylinders with electricity). They pay out at your base but do not count toward secret progress. If they were carrying an actual secret, you can steal it.</P>
            </div>
            <Note>You must deploy your base door before hacking counts toward a secret. Hacking while a secret location is already being determined still earns you files and pay — just not secret progress.</Note>
          </Section>

          <Section id="weapons" icon="◈" title="Weapons">
            <P>Fire with <Key>Ctrl</Key>. Cycle weapons with <Key>Numpad 0</Key>. You auto-switch when ammo runs out. You start with blaster (infinite), laser, and rockets.</P>
            <div className="space-y-4 mt-2">
              {[
                { name: 'Blaster', detail: 'Infinite ammo. Fires red projectiles quickly at short range. Weak but always available — good for suppressing fire.', img: null as string | null },
                { name: 'Laser', detail: 'Limited ammo. Fires slower but deals heavy shield damage at long range. Your primary weapon for duels.', img: '/silencer/lasershot.jpg' },
                { name: 'Rocket Launcher', detail: 'Slow fire rate, limited ammo. One or two hits kill most enemies, especially with depleted shields. You get a rocket-cam view to track the shot in flight.', img: '/silencer/rocketshot.jpg' },
                { name: 'Flamer', detail: 'Must be purchased via tech slots. Very short range, stationary fire, long wind-up. Bypasses shields entirely — damages health directly. Best against cornered or slow targets.', img: '/silencer/flamershot.jpg' },
              ].map(w => (
                <div key={w.name} className="flex gap-4 items-start">
                  {w.img ? (
                    <ScreenShot src={w.img} alt={w.name} className="shrink-0 max-h-10 w-auto" />
                  ) : (
                    <div className="shrink-0 w-12" />
                  )}
                  <div>
                    <div className="text-game-primary font-bold mb-0.5">{w.name}</div>
                    <P>{w.detail}</P>
                  </div>
                </div>
              ))}
            </div>
          </Section>

          <Section id="tech" icon="◎" title="Tech &amp; Inventory">
            <P>You have a limited number of tech slots (base: 3, expandable via XP bonuses up to 8). Before each game, assign slots to item types at the TECH screen in the lobby. During the game, buy those items at the Tech Terminal in your base.</P>
            <P>Cycle carried items with <Key>]</Key>, then deploy with <Key>Enter</Key>. You can carry up to 4 of each type.</P>
            <Note>Enemies can sabotage your basement machines, disabling the corresponding tech slot for that game until you repair it at the Tech Terminal.</Note>
            <div className="overflow-x-auto mt-4">
              <table className="w-full text-xs font-mono">
                <thead>
                  <tr className="text-game-muted border-b border-game-border text-left">
                    <th className="py-2 pr-2 w-8"></th>
                    <th className="py-2 pr-3">Item</th>
                    <th className="py-2 pr-3 text-right">Cost</th>
                    <th className="py-2">Description</th>
                  </tr>
                </thead>
                <tbody>
                  <ItemRow name="Laser Ammo"       cost={100}  icon="laserammo.jpg"    desc="Adds 5 laser shots." />
                  <ItemRow name="Rocket Ammo"      cost={100}  icon="rocketammo.jpg"   desc="Adds 3 rockets." />
                  <ItemRow name="Flamer Ammo"      cost={200}                          desc="Adds 15 flamer fuel." />
                  <ItemRow name="Base Defense"     cost={100}  icon="basedeficon.jpg"  desc="Activates laser turrets around your base. Multiple purchases increase turret durability. Does not detect BLACKROSE agents." />
                  <ItemRow name="Base Door"        cost={300}  icon="basedooricon.jpg" desc="Move your base door when not carrying or homing in on a secret." />
                  <ItemRow name="Camera"           cost={100}                          desc="Deployable camera for remote viewing." />
                  <ItemRow name="Fixed Cannon"     cost={300}  icon="cannonicon.jpg"   desc="Automated laser turret, fires left or right at head or chest height." />
                  <ItemRow name="Flare"            cost={200}  icon="flareicon.jpg"    desc="Spouts flames for several seconds. Blocks corridors; effective against slow or cornered targets." />
                  <ItemRow name="Health Pack"      cost={100}  icon="healthicon.jpg"   desc="Restores health." agency="NOXIS" />
                  <ItemRow name="Lazarus Tract"    cost={300}  icon="laztracticon.jpg" desc="Converts a civilian into a walking bomb — explodes on contact with any agent." agency="LAZARUS" />
                  <ItemRow name="Plasma Bomb"      cost={200}  icon="bombicon.jpg"     desc="Explosive grenade, detonates ~3 seconds after throwing." />
                  <ItemRow name="Shaped Bomb"      cost={100}  icon="bombicon.jpg"     desc="Like a plasma bomb but explosion is directed upward." />
                  <ItemRow name="Plasma Detonator" cost={150}  icon="plasdeticon.jpg"  desc="Deployable explosive with a camera. Detonate remotely with the fire key." />
                  <ItemRow name="EMP Bomb"         cost={1000} icon="bombicon.jpg"     desc="Drains shields of all nearby enemies." />
                  <ItemRow name="Neutron Bomb"     cost={4000} icon="bombicon.jpg"     desc="Kills all guards and agents not inside a base." />
                  <ItemRow name="Poison"           cost={100}  icon="poisonicon.jpg"   desc="Deployed on an enemy — drains their health until they die or heal at their base." agency="BLACKROSE" />
                  <ItemRow name="Poison Flare"     cost={200}  icon="poisflareicon.jpg" desc="Like a flare but deploys poison on contact." agency="BLACKROSE" />
                  <ItemRow name="Security Pass"    cost={1000} icon="secpassicon.jpg"  desc="Government guards ignore you even undisguised." agency="CALIBER" />
                  <ItemRow name="Virus"            cost={400}                          desc="On a robot: it attacks everyone but you. On an enemy base station: disables a tech slot until repaired." agency="STATIC" />
                  <ItemRow name="Insider Info"     cost={500}                          desc="Grants one piece of information toward the next secret. Cannot be used for the final piece." />
                  <ItemRow name="Credit Transfer"  cost={undefined}                    desc="Team games only. Transfers 100 credits to a teammate." />
                </tbody>
              </table>
            </div>
          </Section>

          <Section id="agencies" icon="⬡" title="Agencies">
            <P>Choose your agency when creating an agent. Each has a unique starting bonus and preferred playstyle. You can have agents in multiple agencies and switch between games.</P>
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-3 mt-3">
              <AgencyCard name="NOXIS" color="border-game-primary/50 text-game-primary" bonus="+5 Jump, +3 Endurance. The frontline brawler — extra health and mobility make them hard to kill. Reportedly favors rocket kills for bonus XP." />
              <AgencyCard name="LAZARUS" color="border-purple-500/50 text-purple-400" bonus="Resurrection: random chance to revive in place instead of respawning at base (with 0 shields). Hard to permanently put down." />
              <AgencyCard name="CALIBER" color="border-yellow-500/50 text-yellow-400" bonus="+3 Contacts (earn 30% more credits for files). Great for buying tech fast. Gets the Security Pass — government guards leave you alone." />
              <AgencyCard name="STATIC" color="border-blue-400/50 text-blue-400" bonus="+3 Hacking, Satellite ability (occasionally see all enemies on mini-map). Best hackers — gather intelligence faster than any other agency." img="/silencer/static.jpg" />
              <AgencyCard name="BLACKROSE" color="border-red-500/50 text-red-400" bonus="+2 Shield, full stealth: invisible on radar, base defenses ignore them. Cannot work in teams — lone wolf specialists." />
            </div>
            <div className="mt-4 border-t border-game-border/40 pt-4">
              <div className="text-game-primary mb-2">XP Bonuses</div>
              <P>Earn XP by winning missions. At XP thresholds, choose a bonus. Each can be applied up to 5 times:</P>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-x-6 gap-y-1 mt-2">
                {[
                  ['Endurance', '+20 max health per rank (up to +100)'],
                  ['Shield', '+20 max shield per rank (up to +100)'],
                  ['Jetpack', 'Longer fuel duration per rank'],
                  ['Tech Slot', 'One extra inventory slot per rank (max 8 total)'],
                  ['Hacking', 'More info per hack per rank'],
                  ['Contacts', '+10% file payout per rank (up to +50%)'],
                ].map(([name, desc]) => (
                  <div key={name} className="flex gap-2">
                    <span className="text-game-primary w-24 shrink-0">{name}</span>
                    <span className="text-game-textDim">{desc}</span>
                  </div>
                ))}
              </div>
            </div>
          </Section>

          <Section id="tricks" icon="◉" title="Disguises &amp; Tricks">
            <P>All agents can disguise as a civilian using <Key>Numpad 5</Key>. You will see your own colored armband — enemies will not. Your disguise breaks if you use your jetpack, fire a weapon, or deploy an item.</P>
            <P>While disguised you walk automatically. Tap a direction to change it; hold to run (slower than normal). The trick to a convincing disguise: act like a civilian. Civilians do not climb ladders, jump, run without reason, or hack. If you see a civilian doing any of these — it is an enemy agent.</P>
            <div className="space-y-2 mt-3">
              <div className="text-game-primary mb-1">Useful tricks</div>
              <ul className="list-disc list-inside space-y-2 text-game-textDim ml-2">
                <li>Ambushing: wait at choke points the enemy must pass through — a well-timed bomb or flare as they run by is very effective.</li>
                <li>Jetpack + secret: carrying a secret slows you on foot but not in the air. Jetpack-hop to move faster and stay harder to hit.</li>
                <li>Free falling: you take zero fall damage. Drop off ledges or press <Key>Space</Key> on a ladder to fall instead of climbing down — much faster.</li>
                <li>Looting bodies: enemies drop their files and inventory when killed. Pick them up for extra credits and ammo.</li>
              </ul>
            </div>
          </Section>

          <Section id="teamwork" icon="◑" title="Teamwork &amp; Chat">
            <P>Open chat with <Key>Tab</Key>. By default messages go to your team only. Press <Key>Tab</Key> again to switch to all-player chat. Press <Key>Enter</Key> to send.</P>
            <P>Coordinate with your teammates — a split approach (one agent hacking and collecting secrets while another harasses enemies and sets traps) is far more effective than everyone doing the same thing.</P>
            <Note>Teams and agencies are different. Two agents from the same agency can be on different teams if they use CHANGE TEAMS in the pre-game lobby. Agents from different agencies can never work together.</Note>
          </Section>

        </main>
      </div>
    </div>
  );
}
