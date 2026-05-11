#include "mission_summary.h"

#include "context.h"
#include "node.h"

#include <cstdio>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

namespace {

// Mirrors MissionSummaryScreen::AddSummaryLine. Pads `name` with spaces so
// the result is exactly maxchars (180 / fontwidth=6 = 30) chars, with the
// value text right-justified at the end.
std::string SummaryLine(const char * name, unsigned int value, bool percentage = false)
{
	char valuetext[16];
	snprintf(valuetext, sizeof(valuetext), "%u%s", value, percentage ? "%" : " ");
	const int maxchars = 30;
	int used = (int)std::char_traits<char>::length(name) + (int)std::char_traits<char>::length(valuetext);
	std::string s = name;
	for(int i = 0; i < maxchars - used; i++) s += " ";
	s += valuetext;
	return s;
}

}  // namespace

Node BuildMissionSummary(const Context & ctx, const MissionSummaryHandlers & handlers)
{
	(void)ctx;
	// Mirrors MissionSummaryScreen::Build. At preview-gate state (post-Build,
	// after Refresh(), pre-Tick) the rendered pixels are:
	//   - Two background sprites (bank=6 idx=0, bank=7 idx=5)
	//   - "Mission Summary" title (bank=135, width=12) at (102, 44).
	//   - "+ 0 XP" XP text (bank=136, width=15) at (422, 45) — stats default
	//     to zero so CalculateExperience() returns 0.
	//   - Textbox lines. The legacy textbox auto-scrolls in AddLine: after
	//     all 60 AddLine calls, `scrolled = text.size()-27` captured before
	//     the final push_back = 59-27 = 32. The visible window therefore
	//     starts at deque[32] ("  Shaped:") and renders 28 lines through
	//     deque[59] ("  Player kills:") at y = 92 + i*11.
	//   - 6 "Current X Level:" left labels at (390, 97+i*46) bank=133 width=6
	//   - 6 level values "3","0","0","3","0","0" (Noxis agency default
	//     upgrades from shared/assets/gas/agencies.json) at (550, 97+i*46).
	//   - "Done" Button (B196x33) at anchor (62, 100).
	// Omitted (no pixels at preview gate):
	//   - "*NEW UPGRADE AVAILABLE*" banner (draw=false — Refresh only flips
	//     it true when upgradeavailable, which requires !user->retrieving;
	//     GetUserInfo() in preview just created the user with retrieving=true).
	//   - Upgrade buttons uid 10..15 (not created for the same reason).
	//   - ScrollBar (draw=false default; never flipped here).
	std::vector<Node> children;

	// Title: "Mission Summary" (15 chars * 12 width = 180; legacy centers
	// around x=192 → x = 192 - 90 = 102, y = 44).
	children.push_back(Label("Mission Summary", /*font_bank=*/135, /*font_width=*/12).at(102, 44));

	// XP text: "+ 0 XP" — len 6, width 15 → x = 467 - (6*15)/2 = 422.
	children.push_back(Label("+ 0 XP", /*font_bank=*/136, /*font_width=*/15).at(422, 45));

	// Textbox visible window. Each entry is the text rendered at line `i`
	// (y = 92 + i*11). Lines with no glyphs (blanks, deque[36]/[42]/[48]/
	// [54]) emit a Label with empty text — DrawText short-circuits on
	// strlen=0, so we skip those.
	auto tb_label = [](const std::string & s, int line) {
		return Label(s, /*font_bank=*/133, /*font_width=*/6).at(89, 92 + line * 11);
	};
	int line = 0;
	children.push_back(tb_label(SummaryLine("  Shaped:", 0), line++));        // deque[32]
	children.push_back(tb_label(SummaryLine("  Flare:", 0), line++));         // [33]
	children.push_back(tb_label(SummaryLine("  Poison Flare:", 0), line++));  // [34]
	children.push_back(tb_label(SummaryLine("  Neutron:", 0), line++));       // [35]
	line++;                                                                    // [36] blank
	children.push_back(tb_label("Blaster", line++));                          // [37]
	children.push_back(tb_label(SummaryLine("  Shots fired:", 0), line++));   // [38]
	children.push_back(tb_label(SummaryLine("  Hits:", 0), line++));          // [39]
	children.push_back(tb_label(SummaryLine("  Accuracy:", 0, true), line++));// [40]
	children.push_back(tb_label(SummaryLine("  Player kills:", 0), line++));  // [41]
	line++;                                                                    // [42] blank
	children.push_back(tb_label("Laser", line++));                            // [43]
	children.push_back(tb_label(SummaryLine("  Shots fired:", 0), line++));   // [44]
	children.push_back(tb_label(SummaryLine("  Hits:", 0), line++));          // [45]
	children.push_back(tb_label(SummaryLine("  Accuracy:", 0, true), line++));// [46]
	children.push_back(tb_label(SummaryLine("  Player kills:", 0), line++));  // [47]
	line++;                                                                    // [48] blank
	children.push_back(tb_label("Rocket", line++));                           // [49]
	children.push_back(tb_label(SummaryLine("  Shots fired:", 0), line++));   // [50]
	children.push_back(tb_label(SummaryLine("  Hits:", 0), line++));          // [51]
	children.push_back(tb_label(SummaryLine("  Accuracy:", 0, true), line++));// [52]
	children.push_back(tb_label(SummaryLine("  Player kills:", 0), line++));  // [53]
	line++;                                                                    // [54] blank
	children.push_back(tb_label("Flamer", line++));                           // [55]
	children.push_back(tb_label(SummaryLine("  Shots fired:", 0), line++));   // [56]
	children.push_back(tb_label(SummaryLine("  Hits:", 0), line++));          // [57]
	children.push_back(tb_label(SummaryLine("  Accuracy:", 0, true), line++));// [58]
	children.push_back(tb_label(SummaryLine("  Player kills:", 0), line++));  // [59]

	// 6 "Current X Level:" left labels + level values. Refresh() at preview
	// gate sets each level overlay text from agency[0] (statsagency=0 default,
	// Noxis): endurance=3, shield=0, jetpack=0, techslots=3, hacking=0,
	// contacts=0. Level text x = 556 - text.length()*6 = 550 for all
	// single-digit values.
	static const char * level_labels[6] = {
		"Current Endurance Level:",
		"Current Shield Level:",
		"Current Jetpack Level:",
		"Current Tech Slot Level:",
		"Current Hacking Level:",
		"Current Contacts Level:",
	};
	static const char * level_values[6] = { "3", "0", "0", "3", "0", "0" };
	for(int i = 0; i < 6; i++){
		int y = 97 + i * 46;
		children.push_back(Label(level_labels[i], /*font_bank=*/133, /*font_width=*/6).at(390, y));
		children.push_back(Label(level_values[i], /*font_bank=*/133, /*font_width=*/6).at(550, y));
	}

	// "Done" button — B196x33 at anchor (62, 100). uid=0; live engine wiring
	// maps this to GoToState(LOBBY) or MAINMENU depending on lobby state.
	children.push_back(
		Button("Done", ButtonType::B196x33).at(62, 100).onClick(handlers.on_done)
	);

	return Background(/*bank=*/6, /*index=*/0, {
		Sprite(/*bank=*/7, /*index=*/5),
		Group(std::move(children)),
	});
}

}  // namespace v2
}  // namespace ui
