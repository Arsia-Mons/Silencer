'use client';
import Link from 'next/link';
import { usePlayerAuth, playerLogout } from '../../lib/auth.js';
import { getMyProfile, getMyMatches } from '../../lib/api.js';
import { useState, useEffect } from 'react';

const AGENCY_NAMES = ['NOXIS', 'LAZARUS', 'CALIBER', 'STATIC', 'BLACKROSE'];
const WEAPON_NAMES = ['Blaster', 'Laser', 'Rocket', 'Flamer'];

function fmtTime(secs) {
  if (!secs) return '0m';
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
}

function StatRow({ label, value, dim }) {
  return (
    <div className="flex justify-between items-center py-1 border-b border-game-border/40">
      <span className="text-xs font-mono text-game-textDim">{label}</span>
      <span className={`text-xs font-mono font-bold ${dim ? 'text-game-muted' : 'text-game-text'}`}>{value ?? 0}</span>
    </div>
  );
}

function Section({ title, icon, children }) {
  return (
    <div className="bg-game-bgCard border border-game-border rounded p-4 mb-4">
      <h2 className="text-game-primary font-mono text-sm tracking-widest mb-3">{icon} {title}</h2>
      {children}
    </div>
  );
}

export default function MePage() {
  usePlayerAuth();
  const [profile, setProfile] = useState(null);
  const [matches, setMatches] = useState([]);
  const [matchPage, setMatchPage] = useState(1);
  const [matchTotal, setMatchTotal] = useState(0);
  const [activeAgency, setActiveAgency] = useState(0);
  const [error, setError] = useState(null);

  const playerName = typeof window !== 'undefined'
    ? JSON.parse(localStorage.getItem('zs_player') || '{}').name : '';

  useEffect(() => {
    getMyProfile().then(setProfile).catch(e => setError(e.message));
  }, []);

  useEffect(() => {
    getMyMatches(matchPage).then(r => { setMatches(r.matches); setMatchTotal(r.total); })
      .catch(() => {});
  }, [matchPage]);

  const ls = profile?.lifetimeStats || {};
  const agency = profile?.agencies?.[activeAgency] || {};

  return (
    <div className="min-h-screen">
      {/* Top bar */}
      <header className="flex items-center justify-between px-6 py-4 border-b border-game-border bg-game-bgCard">
        <div>
          <img src="/logo.png" alt="Silencer" className="h-10 w-auto" />
          <div className="text-game-textDim text-xs font-mono tracking-widest">PLAYER PORTAL</div>
        </div>
        <div className="flex items-center gap-4">
          <span className="text-game-text font-mono text-sm">◈ {playerName}</span>
          <Link href="/gamestats"
            className="px-3 py-1.5 text-xs font-mono text-game-textDim border border-game-border rounded hover:border-game-primary hover:text-game-primary transition-colors">
            [ GAME STATS ]
          </Link>
          <Link href="/howto"
            className="px-3 py-1.5 text-xs font-mono text-game-textDim border border-game-border rounded hover:border-game-primary hover:text-game-primary transition-colors">
            [ HOW TO PLAY ]
          </Link>
          <button onClick={playerLogout}
            className="px-3 py-1.5 text-xs font-mono text-game-muted border border-game-border rounded hover:border-game-danger hover:text-game-danger transition-colors">
            [ LOGOUT ]
          </button>
        </div>
      </header>

      <main className="max-w-5xl mx-auto p-6">
        {error && <div className="bg-red-900/20 border border-game-danger text-game-danger text-xs font-mono px-4 py-2 rounded mb-4">{error}</div>}

        {profile && (
          <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
            {/* Left column */}
            <div className="lg:col-span-1 space-y-4">

              {/* Identity */}
              <Section title="IDENTITY" icon="◈">
                <StatRow label="CALLSIGN" value={profile.name} />
                <StatRow label="ACCOUNT ID" value={`#${profile.accountId}`} />
                <StatRow label="FIRST SEEN" value={new Date(profile.firstSeen).toLocaleDateString()} />
                <StatRow label="LAST SEEN" value={new Date(profile.lastSeen).toLocaleDateString()} />
                <StatRow label="TOTAL LOGINS" value={profile.loginCount} />
                <StatRow label="PLAYTIME" value={fmtTime(profile.totalPlaytimeSecs)} />
                {profile.banned && (
                  <div className="mt-2 text-game-danger text-xs font-mono border border-game-danger/30 rounded px-2 py-1">
                    ⚠ ACCOUNT SUSPENDED{profile.banReason ? `: ${profile.banReason}` : ''}
                  </div>
                )}
              </Section>

              {/* Agency selector */}
              <Section title="AGENCIES" icon="⬡">
                <div className="flex flex-wrap gap-1 mb-3">
                  {AGENCY_NAMES.map((n, i) => (
                    <button key={i} onClick={() => setActiveAgency(i)}
                      className={`px-2 py-1 text-xs font-mono rounded border transition-colors
                        ${activeAgency === i
                          ? 'bg-game-dark border-game-primary text-game-primary'
                          : 'border-game-border text-game-textDim hover:text-game-text'}`}>
                      {n}
                    </button>
                  ))}
                </div>
                <div className="text-xs font-mono text-game-textDim mb-2">AGENCY {AGENCY_NAMES[activeAgency].toUpperCase()} — LVL {agency.level ?? 0}</div>
                <StatRow label="WINS" value={agency.wins} />
                <StatRow label="LOSSES" value={agency.losses} />
                <StatRow label="XP TO NEXT" value={agency.xpToNextLevel} />
                <div className="mt-2 pt-2 border-t border-game-border/40">
                  <div className="text-xs font-mono text-game-muted mb-1">UPGRADES</div>
                  <StatRow label="Endurance"  value={`${agency.endurance ?? 0}/5`} />
                  <StatRow label="Shield"     value={`${agency.shield ?? 0}/5`} />
                  <StatRow label="Jetpack"    value={`${agency.jetpack ?? 0}/5`} />
                  <StatRow label="Tech Slots" value={`${agency.techSlots ?? 3}/8`} />
                  <StatRow label="Hacking"    value={`${agency.hacking ?? 0}/5`} />
                  <StatRow label="Contacts"   value={`${agency.contacts ?? 0}/5`} />
                </div>
              </Section>
            </div>

            {/* Right column */}
            <div className="lg:col-span-2 space-y-4">

              {/* Combat lifetime stats */}
              <Section title="LIFETIME COMBAT" icon="◉">
                <div className="grid grid-cols-2 gap-x-6">
                  <div>
                    <StatRow label="KILLS"    value={ls.kills} />
                    <StatRow label="DEATHS"   value={ls.deaths} />
                    <StatRow label="K/D RATIO" value={ls.deaths ? (ls.kills / ls.deaths).toFixed(2) : (ls.kills || 0)} />
                    <StatRow label="SUICIDES" value={ls.suicides} dim />
                    <StatRow label="POISONS"  value={ls.poisons} dim />
                  </div>
                  <div>
                    <StatRow label="GUARDS KILLED"   value={ls.guardsKilled} />
                    <StatRow label="ROBOTS KILLED"   value={ls.robotsKilled} />
                    <StatRow label="DEFENSE KILLED"  value={ls.defenseKilled} />
                    <StatRow label="CIVILIANS KILLED" value={ls.civiliansKilled} dim />
                  </div>
                </div>
              </Section>

              {/* Weapons */}
              <Section title="WEAPONS" icon="◧">
                <div className="overflow-x-auto">
                  <table className="w-full text-xs font-mono">
                    <thead>
                      <tr className="text-game-muted border-b border-game-border">
                        <th className="text-left py-1">WEAPON</th>
                        <th className="text-right py-1">FIRES</th>
                        <th className="text-right py-1">HITS</th>
                        <th className="text-right py-1">ACC%</th>
                        <th className="text-right py-1">PK</th>
                      </tr>
                    </thead>
                    <tbody>
                      {[
                        { label: 'Blaster', fires: ls.blasterFires, hits: ls.blasterHits, pk: ls.blasterKills },
                        { label: 'Laser',   fires: ls.laserFires,   hits: ls.laserHits,   pk: ls.laserKills },
                        { label: 'Rocket',  fires: ls.rocketFires,  hits: ls.rocketHits,  pk: ls.rocketKills },
                        { label: 'Flamer',  fires: ls.flamerFires,  hits: ls.flamerHits,  pk: ls.flamerKills },
                      ].map(w => (
                        <tr key={w.label} className="border-b border-game-border/30 text-game-text">
                          <td className="py-1 text-game-textDim">{w.label}</td>
                          <td className="text-right py-1">{w.fires || 0}</td>
                          <td className="text-right py-1">{w.hits || 0}</td>
                          <td className="text-right py-1">{w.fires ? ((w.hits / w.fires) * 100).toFixed(1) : '—'}</td>
                          <td className="text-right py-1 text-game-primary">{w.pk || 0}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </Section>

              {/* Objectives & tech */}
              <Section title="OBJECTIVES & TECH" icon="◎">
                <div className="grid grid-cols-2 gap-x-6">
                  <div>
                    <div className="text-xs font-mono text-game-muted mb-1">SECRETS</div>
                    <StatRow label="Picked up"  value={ls.secretsPickedUp} />
                    <StatRow label="Returned"   value={ls.secretsReturned} />
                    <StatRow label="Stolen"     value={ls.secretsStolen} />
                    <StatRow label="Dropped"    value={ls.secretsDropped} dim />
                    <div className="text-xs font-mono text-game-muted mt-2 mb-1">HACKING</div>
                    <StatRow label="Files Hacked"   value={ls.filesHacked} />
                    <StatRow label="Files Returned" value={ls.filesReturned} />
                    <StatRow label="Viruses Used"   value={ls.virusesUsed} />
                    <div className="text-xs font-mono text-game-muted mt-2 mb-1">ECONOMY</div>
                    <StatRow label="Credits Earned" value={ls.creditsEarned} />
                    <StatRow label="Credits Spent"  value={ls.creditsSpent} />
                    <StatRow label="Heals Done"     value={ls.healsDone} />
                  </div>
                  <div>
                    <div className="text-xs font-mono text-game-muted mb-1">TECH / THROWABLES</div>
                    <StatRow label="Grenades"     value={ls.grenadesThrown} />
                    <StatRow label="Neutrons"     value={ls.neutronsThrown} />
                    <StatRow label="EMPs"         value={ls.empsThrown} />
                    <StatRow label="Shaped"       value={ls.shapedThrown} />
                    <StatRow label="Plasmas"      value={ls.plasmasThrown} />
                    <StatRow label="Flares"       value={ls.flaresThrown} />
                    <StatRow label="Poison Flares" value={ls.poisonFlaresThrown} />
                    <StatRow label="Tractor Beams" value={ls.tractsPlanted} />
                    <StatRow label="Cameras"      value={ls.camerasPlanted} />
                    <StatRow label="Detonators"   value={ls.detsPlanted} />
                    <StatRow label="Cannons Placed"    value={ls.fixedCannonsPlaced} />
                    <StatRow label="Cannons Destroyed" value={ls.fixedCannonsDestroyed} />
                    <StatRow label="Health Packs"      value={ls.healthPacksUsed} />
                    <StatRow label="Powerups"          value={ls.powerupsPickedUp} />
                  </div>
                </div>
              </Section>

              {/* Match history */}
              <Section title={`MATCH HISTORY (${matchTotal})`} icon="◑">
                {matches.length === 0
                  ? <p className="text-game-muted text-xs font-mono">NO MATCHES RECORDED YET</p>
                  : <>
                    <div className="overflow-x-auto">
                      <table className="w-full text-xs font-mono">
                        <thead>
                          <tr className="text-game-muted border-b border-game-border">
                            <th className="text-left py-1">DATE</th>
                            <th className="text-left py-1">RESULT</th>
                            <th className="text-right py-1">K</th>
                            <th className="text-right py-1">D</th>
                            <th className="text-right py-1">XP</th>
                            <th className="text-right py-1">HACKED</th>
                            <th className="text-right py-1">SECRETS</th>
                          </tr>
                        </thead>
                        <tbody>
                          {matches.map(m => (
                            <tr key={m._id} className="border-b border-game-border/30 hover:bg-game-bgHover">
                              <td className="py-1 text-game-textDim">{new Date(m.createdAt).toLocaleDateString()}</td>
                              <td className={`py-1 font-bold ${m.won ? 'text-game-primary' : 'text-game-danger'}`}>
                                {m.won ? 'WIN' : 'LOSS'}
                              </td>
                              <td className="text-right py-1 text-game-text">{m.kills || 0}</td>
                              <td className="text-right py-1 text-game-muted">{m.deaths || 0}</td>
                              <td className="text-right py-1 text-cyan-400">{m.xp || 0}</td>
                              <td className="text-right py-1">{m.filesHacked || 0}</td>
                              <td className="text-right py-1">{m.secretsReturned || 0}</td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                    {/* Pagination */}
                    {matchTotal > 20 && (
                      <div className="flex gap-2 justify-center mt-3">
                        <button onClick={() => setMatchPage(p => Math.max(1, p - 1))} disabled={matchPage === 1}
                          className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary disabled:opacity-30">‹ PREV</button>
                        <span className="text-xs font-mono text-game-muted py-1">
                          {matchPage} / {Math.ceil(matchTotal / 20)}
                        </span>
                        <button onClick={() => setMatchPage(p => p + 1)} disabled={matchPage >= Math.ceil(matchTotal / 20)}
                          className="px-3 py-1 text-xs font-mono border border-game-border text-game-textDim rounded hover:border-game-primary disabled:opacity-30">NEXT ›</button>
                      </div>
                    )}
                  </>
                }
              </Section>
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
