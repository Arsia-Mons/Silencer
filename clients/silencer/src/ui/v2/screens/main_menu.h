#ifndef SILENCER_UI_V2_SCREENS_MAIN_MENU_H
#define SILENCER_UI_V2_SCREENS_MAIN_MENU_H

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Returns the declarative tree for the main menu. Pure function: same
// inputs produce the same tree. Layout values mirror the legacy
// MainMenuScreen exactly (clients/silencer/src/ui/screens/main_menu/
// main_menu_screen.cpp) so the rendered output is byte-identical at
// scale=1; the preview harness verifies this with PPM diff.
Node BuildMainMenu(const Context & ctx);

}  // namespace v2
}  // namespace ui

#endif
