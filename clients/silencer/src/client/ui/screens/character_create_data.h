#pragma once

// SIL-101 / parity: the Character Create "Select Agency" detail content,
// ported verbatim from origin/main character_create_layout.cpp kAgencies.
// Advantage rows split origin's "Label +N" lines into label + bonus (the
// brackets render as bank-134 glyph sprites, not text); "... Ability" lines
// are label-only and origin orders them after the +N rows. The description is
// origin's lore paragraph, word-wrapped by the text engine exactly as origin's
// DetailParagraph (TextWrap::Words) wraps it in the 196-wide detail column.

namespace client::ui {

struct Advantage {
  const char *label; // e.g. "Endurance"
  const char *bonus; // e.g. "+3" (nullptr => label-only ability row)
};

struct AgencyInfo {
  const char *name;
  const char *control_id;
  static constexpr int kMaxAdvantages = 4;
  Advantage advantages[kMaxAdvantages]; // trailing {nullptr,nullptr} => unused
  const char *description;              // one paragraph; the engine wraps it
};

constexpr int kAgencyCount = 5;

constexpr AgencyInfo kAgencies[kAgencyCount] = {
    {"Noxis", "AgencyNoxis",
     {{"Endurance", "+3"}, {"Jump", "+5"}, {nullptr, nullptr}, {nullptr, nullptr}},
     "The Noxis corporation terraformed the majority of the initial habitable sectors of Mars, and continues to do so, as well as producing and selling 70 percent of the breathable oxygen. Since they are widely known to the populace and government, they have taken steps to bolster their agent's health so they are better able to avoid detection. Training in bio-sporria rich environments, and possessing suits with advanced oxygen processors and filters, has given these agents improved physical abilities such as higher jumps, more stamina, and enhanced durability."},
    {"Lazarus", "AgencyLazarus",
     {{"Resurrection Ability", nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}},
     "Like the mythical phoenix, the loose organization known as Lazarus has been resurrected every hundred years and causing chaos in the larger urban areas of Mars. Whether it's belief in some higher power, an understanding of mystical truths, or something else entirely, no one outside of the group is sure of this Lazarus. Numerous witnesses of Lazarus attacks claim to have seen supposedly fatally wounded agents continue themselves off and continue their efforts, virtually undamaged."},
    {"Caliber", "AgencyCaliber",
     {{"Contacts", "+3"}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}},
     "Caliber agents specialize in access, favors, and fast-moving procurement. Their contacts let them gather better mission rewards and keep security pressure away from their teams for longer than most agencies can manage."},
    {"Static", "AgencyStatic",
     {{"Hacking", "+3"}, {"Satellite Ability", nullptr}, {nullptr, nullptr}, {nullptr, nullptr}},
     "The Guv first realized that there was a brain-drain about four years ago. Silently, the youngest talented computer specialists and high level programmers were disappearing and leaving the Guv without the technical support it required. They ended up creating Static as a technological savvy, but anti-government agency, but many members are more interested in reinforcing their reputation as the best skilled hackers with the best skills and techniques."},
    {"Black Rose", "AgencyBlackRose",
     {{"Shield", "+2"}, {"Stealth Ability", nullptr}, {nullptr, nullptr}, {nullptr, nullptr}},
     "More like a dark force than a loose collection of humans, little is known of Black Rose. Something vibrant was taken from these people, and left in its place was a new form of sustenance. Its members seem to be misanthropic and have a penchant for using illegal compounds such as Milloxthal injections to cause extreme pain and internal withering in their victims. Shunned by even the most avaristic mercenaries, these agents work alone and yet still prosper due to some combination of ego and superlative talent."},
};

} // namespace client::ui
