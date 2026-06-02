#pragma once

// SIL-101: the Character Create agency detail table. Names + agency index match
// CreateCharacter / Team order (Noxis 0 .. Black Rose 4). The numeric stats are
// the live upgrade caps + free-point counts from shared/assets/gas/agencies.json
// (kept in sync here for the detail column without reaching into the GAS loader
// from the app shell). The tagline/description prose is derived from each
// agency's stat profile — refine against origin/main's kAgencies copy when it is
// available.

namespace client::ui {

struct AgencyInfo {
  const char *name;
  const char *control_id;
  const char *tagline;      // one-line advantage summary
  const char *security;     // squad / deployment line
  const char *stats[4];     // four-line upgrade-cap + free-point block
  const char *description;  // flavor blurb
};

constexpr int kAgencyCount = 5;

constexpr AgencyInfo kAgencies[kAgencyCount] = {
    {"Noxis", "AgencyNoxis", "Heavy assault", "Squad: 4 per team",
     {"Endurance 8   Shield 5", "Jetpack 5   Tech 8", "Hacking 5   Contacts 5",
      "Free points: 6"},
     "Top endurance & tech."},
    {"Lazarus", "AgencyLazarus", "Lean, no waste", "Squad: 4 per team",
     {"Endurance 5   Shield 5", "Jetpack 5   Tech 8", "Hacking 5   Contacts 5",
      "Free points: 3"},
     "Disciplined economy."},
    {"Caliber", "AgencyCaliber", "Networked", "Squad: 4 per team",
     {"Endurance 5   Shield 5", "Jetpack 5   Tech 8", "Hacking 5   Contacts 8",
      "Free points: 6"},
     "Deep contact network."},
    {"Static", "AgencyStatic", "Infiltration", "Squad: 4 per team",
     {"Endurance 5   Shield 5", "Jetpack 5   Tech 8", "Hacking 8   Contacts 5",
      "Free points: 6"},
     "Owns map systems."},
    {"Black Rose", "AgencyBlackRose", "Lone wolf", "Squad: 1 (solo)",
     {"Endurance 5   Shield 7", "Jetpack 5   Tech 8", "Hacking 5   Contacts 5",
      "Free points: 5"},
     "Solo, heavy shield."},
};

} // namespace client::ui
