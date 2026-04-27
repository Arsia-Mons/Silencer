#include "demo_data.h"

namespace silencer::demo {

CharacterView Character() {
  return CharacterView{
      .username = "demo",
      .level = 8,
      .wins = 47,
      .losses = 12,
      .xp_to_next = 220,
  };
}

const char *ChannelName() { return "Lobby"; }

const std::vector<std::string> &ChatLines() {
  // Oldest-first; rendered bottom-to-top in the chat scrollback.
  static const std::vector<std::string> kLines = {
      "Vector: anyone up for a round?",
      "Solace: still waiting on Krieg's match to finish",
      "Ember: we got 4 in casual #1",
      "Vector: joining",
      "Halcyon: gg everyone",
  };
  return kLines;
}

const std::vector<std::string> &PresenceLines() {
  // Section headers + member rows in the order the lobby server emits them.
  static const std::vector<std::string> kLines = {
      "In Lobby",
      "Ember",
      "Halcyon",
      "Solace",
      "Vector",
      "demo",
      "Pregame",
      "Quill -Capture the Tag-",
      "Playing",
      "Krieg -Casual Match #1-",
  };
  return kLines;
}

const std::vector<std::string> &GameRows() {
  static const std::vector<std::string> kRows = {
      "Veterans Only",
      "Tutorial",
      "Capture the Tag",
      "Casual Match #1",
  };
  return kRows;
}

const std::vector<std::string> &MapRows() {
  static const std::vector<std::string> kRows = {
      "ALLY10c",
      "CRAN01h",
      "EASY05c",
      "PIT16d",
  };
  return kRows;
}

}  // namespace silencer::demo
