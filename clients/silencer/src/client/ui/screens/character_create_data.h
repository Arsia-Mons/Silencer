#pragma once

// SIL-101 / parity: the Character Create "Select Agency" detail content. Names +
// agency index match CreateCharacter / Team order (Noxis 0 .. Black Rose 4). The
// reference (origin/main) detail column is an "Advantages" bonus list followed by
// a "Description" lore paragraph — NOT a raw upgrade-cap stat dump. The advantage
// bonuses are curated from each agency's distinctive GAS defaultUpgrades; the
// description is split into fixed-slot lines (the engine paints '\n' as a single
// glyph and the wrap path is unused by screens, so multi-line prose is authored
// as one BodyText per line — trailing nullptr terminates the list).

namespace client::ui {

struct Advantage {
  const char *label; // e.g. "Endurance"
  const char *bonus; // e.g. "[+3]"  (nullptr => unused slot)
};

struct AgencyInfo {
  const char *name;
  const char *control_id;
  static constexpr int kMaxAdvantages = 4;
  static constexpr int kMaxDescLines = 24;
  Advantage advantages[kMaxAdvantages]; // trailing {nullptr,nullptr} => unused
  const char *description[kMaxDescLines]; // trailing nullptr => end of prose
};

constexpr int kAgencyCount = 5;

constexpr AgencyInfo kAgencies[kAgencyCount] = {
    {"Noxis", "AgencyNoxis",
     {{"Endurance", "[+3]"}, {"Jump", "[+5]"}, {nullptr, nullptr}, {nullptr, nullptr}},
     {"The Noxis corporation",
      "terraformed the majority of",
      "the initial habitable",
      "sectors of Mars, and",
      "continues to do so, as well",
      "as producing and selling 70",
      "percent of the breathable",
      "oxygen. Since they are",
      "widely known to the",
      "populace and government,",
      "they have taken steps to",
      "bolster their agent's",
      "health so they are better",
      "able to avoid detection.",
      "Training in bio-sporria",
      "rich environments, and",
      "possessing suits with",
      "advanced oxygen processors",
      "and filters, has given",
      "these agents improved",
      "physical abilities such as",
      "higher jumps, more stamina,",
      "and enhanced durability.", nullptr}},
    {"Lazarus", "AgencyLazarus",
     {{"Tech", "[+3]"}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}},
     {"The Lazarus agency runs a lean,",
      "disciplined economy. Every agent",
      "wastes nothing and squeezes full",
      "value from minimal tech - reliable",
      "operators in any theater.", nullptr}},
    {"Caliber", "AgencyCaliber",
     {{"Contacts", "[+3]"}, {"Tech", "[+3]"}, {nullptr, nullptr}, {nullptr, nullptr}},
     {"Caliber maintains the deepest",
      "contact network on Mars. Their",
      "agents call in favors, intel, and",
      "supply drops others cannot reach,",
      "turning relationships into a",
      "battlefield advantage.", nullptr}},
    {"Static", "AgencyStatic",
     {{"Hacking", "[+3]"}, {"Tech", "[+3]"}, {nullptr, nullptr}, {nullptr, nullptr}},
     {"Static specializes in infiltration",
      "and systems control. Their agents",
      "own the map's terminals, doors, and",
      "defenses, slipping past what stops",
      "everyone else.", nullptr}},
    {"Black Rose", "AgencyBlackRose",
     {{"Shield", "[+2]"}, {"Tech", "[+3]"}, {nullptr, nullptr}, {nullptr, nullptr}},
     {"Black Rose deploys lone wolves.",
      "Each agent operates solo behind",
      "heavy shielding, trading squad",
      "support for resilience and the",
      "freedom to strike without backup.", nullptr}},
};

} // namespace client::ui
