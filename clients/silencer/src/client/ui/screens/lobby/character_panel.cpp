#include "character_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include "clay/clay.h"
#include "clay_ui_payloads.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/box.h"
#include "primitives/text.h"

#include <algorithm>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextAlign;
using silencer::ui::primitives::TextOpts;
using silencer::ui::primitives::TextAdvance;
using silencer::ui::primitives::TextLineHeight;
using silencer::ui::primitives::MeasureText;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::TextWrap;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Box;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

namespace silencer::client_ui::lobby {
    namespace character_panel_detail {
        constexpr const char *kActionAgents = "lobby.character.agents";
        constexpr uint16_t kPanelPad = 6;
        constexpr uint16_t kDetailsGap = 5;
        constexpr uint16_t kStatRowGap = 2;
        constexpr uint16_t kEmblemGap = 10;
        constexpr uint16_t kInfoGap = 10;
        constexpr uint16_t kButtonHeight = 21;
        constexpr uint16_t kUpgradePanelPadX = 4;
        constexpr uint16_t kUpgradePanelPadY = 3;
        constexpr uint16_t kUpgradePanelHeight = 88;
        constexpr uint16_t kUpgradeRowHeight = 12;
        constexpr int kUpgradePanelMinWidth = 96;
        constexpr int kUpgradeValueColumnMinWidth = 18;
        constexpr int kActionButtonMinWidth = 92;
        constexpr int kActionButtonPaddingX = 12;
        constexpr uint16_t kAgencySpriteBank = 181;
        constexpr int kEmblemWidthPct = 18;
        constexpr int kEmblemMinWidth = 40;
        constexpr int kEmblemMaxWidth = 64;

        // Per-frame text buffers. The layout pass keeps pointers to these for the
        // duration of the layout, so they MUST live past BuildCharacterPanelTree's
        // return. Static-lifetime works because the layout consumes them
        // synchronously inside the caller's BeginLayout/EndLayout window.
        struct StatsBuffers {
            std::string name;
            std::string level;
            std::string wins;
            std::string losses;
            std::string xp;
            std::string endurance;
            std::string shield;
            std::string jetpack;
            std::string techslots;
            std::string hacking;
            std::string contacts;
        };

        StatsBuffers g_stats;

        Clay_String FromStd(const std::string &s) {
            Clay_String cs;
            cs.isStaticallyAllocated = false;
            cs.length = static_cast<int32_t>(s.size());
            cs.chars = s.c_str();
            return cs;
        }

        Clay_String FromCStr(const char *s) {
            Clay_String cs;
            cs.isStaticallyAllocated = false;
            cs.length = static_cast<int32_t>(std::strlen(s));
            cs.chars = s;
            return cs;
        }

        int ClampInt(int value, int lo, int hi) {
            return std::max(lo, std::min(value, hi));
        }

        std::string FitMiddleEllipsis(const std::string &text,
                                      TextSize size,
                                      int availablePx) {
            const int advance = std::max(1, static_cast<int>(TextAdvance(size)));
            const int maxChars = availablePx / advance;
            if (maxChars <= 0) return "";
            if (static_cast<int>(text.size()) <= maxChars) return text;
            if (maxChars <= 3) return text.substr(0, static_cast<size_t>(maxChars));

            const int kept = maxChars - 3;
            const int front = (kept + 1) / 2;
            const int back = kept - front;
            return text.substr(0, static_cast<size_t>(front)) + "..." +
                   text.substr(text.size() - static_cast<size_t>(back));
        }

        int BodyTextWidth(const std::string &text) {
            return static_cast<int>(
                MeasureText(FromStd(text), TextSize::Body).width);
        }

        // One stat row: a fixed-width label column followed by its value, kept as a
        // tight pair (the Game Options label/value convention). The fixed label
        // column aligns all three values into a clean column. Rows grow to fill the
        // table height; styling is default Body text.
        void StatRow(int index,
                     const char *label,
                     const std::string &value,
                     int labelColumnWidth) {
            CLAY({ .id = CLAY_IDI("CharacterPanelStatRow", index),
                 .layout = {
                 .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) },
                 .childGap = 8,
                 .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                 .y = CLAY_ALIGN_Y_CENTER },
                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                 },
                 .clip = { .horizontal = true } }) {
                CLAY({ .id = CLAY_IDI("CharacterPanelStatLabel", index),
                     .layout = {
                     .sizing = { CLAY_SIZING_FIXED(static_cast<float>(labelColumnWidth)),
                     CLAY_SIZING_GROW(0) },
                     .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                     .y = CLAY_ALIGN_Y_CENTER },
                     },
                     .clip = { .horizontal = true } }) {
                    Text(FromCStr(label),
                         TextOpts{.size = TextSize::Body, .wrap = TextWrap::None});
                }
                Text(FromStd(value),
                     TextOpts{.size = TextSize::Body, .wrap = TextWrap::None});
            }
        }

        void UpgradeRow(int index,
                        const char *label,
                        const std::string &value,
                        int valueColumnWidth) {
            CLAY({ .id = CLAY_IDI("CharacterPanelUpgradeRow", index),
                 .layout = {
                 .sizing = {
                 CLAY_SIZING_GROW(0),
                 CLAY_SIZING_FIXED(static_cast<float>(kUpgradeRowHeight)),
                 },
                 .childGap = 6,
                 .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                 .y = CLAY_ALIGN_Y_CENTER },
                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                 },
                 .clip = { .horizontal = true } }) {
                CLAY({ .id = CLAY_IDI("CharacterPanelUpgradeLabel", index),
                     .layout = {
                     .sizing = { CLAY_SIZING_GROW(0),
                     CLAY_SIZING_GROW(0) },
                     .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                     .y = CLAY_ALIGN_Y_CENTER },
                     },
                     .clip = { .horizontal = true } }) {
                    Text(FromCStr(label),
                         TextOpts{.size = TextSize::Body, .wrap = TextWrap::None});
                }

                CLAY({ .id = CLAY_IDI("CharacterPanelUpgradeValue", index),
                     .layout = {
                     .sizing = {
                     CLAY_SIZING_FIXED(static_cast<float>(valueColumnWidth)),
                     CLAY_SIZING_GROW(0),
                     },
                     .childAlignment = { .x = CLAY_ALIGN_X_RIGHT,
                     .y = CLAY_ALIGN_Y_CENTER },
                     },
                     .clip = { .horizontal = true } }) {
                    Text(FromStd(value),
                         TextOpts{
                             .size = TextSize::Body,
                             .align = TextAlign::Right,
                             .wrap = TextWrap::None
                         });
                }
            }
        }

        // Render the agency crest as a scaling IMAGE element: it fills a fixed-width,
        // body-height box and the compositor scales the sprite up (Contain, crisp
        // nearest sampling) in its own palette — no green tint, no native-size cap.
        void AgencyEmblem(Uint8 agency, int boxWidth) {
            CLAY({ .id = CLAY_ID("CharacterPanelAgencyEmblem"),
                 .layout = {
                 .sizing = { CLAY_SIZING_FIXED(static_cast<float>(boxWidth)),
                 CLAY_SIZING_FIXED(static_cast<float>(boxWidth)) },
                 },
                 .image = { .imageData = silencer::clay_bridge::PackImageContain(
                     static_cast<Uint8>(kAgencySpriteBank), agency) } }) {
            }
        }

        void LevelBadge(const std::string &level, int width) {
            CLAY({ .id = CLAY_ID("CharacterPanelLevelBadge"),
                 .layout = {
                 .sizing = { CLAY_SIZING_FIXED(static_cast<float>(width)),
                 CLAY_SIZING_FIXED(static_cast<float>(
                     TextLineHeight(TextSize::Body))) },
                 .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
                 .y = CLAY_ALIGN_Y_CENTER },
                 },
                 .clip = { .horizontal = true } }) {
                Text(FromStd(level),
                     TextOpts{
                         .size = TextSize::Body,
                         .align = TextAlign::Center,
                         .wrap = TextWrap::None
                     });
            }
        }

        void XpLine(const std::string &xp) {
            CLAY({ .id = CLAY_ID("CharacterPanelXpLine"),
                 .layout = {
                 .sizing = { CLAY_SIZING_GROW(0),
                 CLAY_SIZING_FIXED(static_cast<float>(
                     TextLineHeight(TextSize::Body))) },
                 .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                 .y = CLAY_ALIGN_Y_CENTER },
                 },
                 .clip = { .horizontal = true } }) {
                Text(FromStd(xp),
                     TextOpts{
                         .size = TextSize::Body,
                         .wrap = TextWrap::None
                     });
            }
        }

        void UpgradePanel(int valueColumnWidth) {
            CLAY(Box(BoxVariants::Inset, {
                .id = CLAY_ID("CharacterPanelUpgradePanel"),
                .layout = {
                .sizing = { CLAY_SIZING_GROW(0),
                CLAY_SIZING_GROW(0) },
                .padding = {
                kUpgradePanelPadX,
                kUpgradePanelPadX,
                kUpgradePanelPadY,
                kUpgradePanelPadY,
                },
                .childGap = character_panel_detail::kStatRowGap,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .clip = { .horizontal = true, .vertical = true },
                })) {
                UpgradeRow(10, "ENDURANCE:", g_stats.endurance, valueColumnWidth);
                UpgradeRow(11, "SHIELD:", g_stats.shield, valueColumnWidth);
                UpgradeRow(12, "JETPACK:", g_stats.jetpack, valueColumnWidth);
                UpgradeRow(13, "TECH SLOTS:", g_stats.techslots, valueColumnWidth);
                UpgradeRow(14, "HACKING:", g_stats.hacking, valueColumnWidth);
                UpgradeRow(15, "CONTACTS:", g_stats.contacts, valueColumnWidth);
            }
        }
    } // namespace character_panel_detail

    void CharacterPanelInit(CharacterPanelState &state,
                            LobbyCharacterModel &character) {
        state.selectedAgency = character.default_agency();
        state.lastReconciled = -1; // forces first-frame reconcile pass
        state.newCharacterRequested = false;
    }

    void CharacterPanelTick(CharacterPanelState &state,
                            LobbyCharacterModel &character) {
        state.selectedAgency = character.selected_agency();
        state.agentSelectionLocked = character.agent_selection_locked();
        if (static_cast<int>(state.selectedAgency) != state.lastReconciled) {
            state.lastReconciled = state.selectedAgency;
            character.apply_selected_agency(state.selectedAgency);
        }
    }

    bool CharacterPanelHandleUiIntent(CharacterPanelState &state,
                                      LobbyCharacterModel &character,
                                      const silencer::ui::UiAction &action) {
        if (action.kind != silencer::ui::UiActionKind::Activate) return false;
        if (action.id == character_panel_detail::kActionAgents) {
            if (character.agent_selection_locked()) return true;
            state.newCharacterRequested = true;
            return true;
        }
        return false;
    }

    void BuildCharacterPanelTree(CharacterPanelState &state,
                                 Uint16 panelWidth,
                                 LobbyCharacterModel &character,
                                 silencer::ui::UiInteractionRegistry &interactions) {
        // Refresh display strings each frame. Clay rebuilds this compact panel
        // from scratch, so the buffers only need to remain stable through the
        // current layout pass.
        const LobbyCharacterPanelSnapshot snapshot = character.panel(state.selectedAgency);
        state.agentSelectionLocked = snapshot.agent_selection_locked;
        const Uint8 a = snapshot.agency;
        const int innerWidth = std::max(1,
                                        static_cast<int>(panelWidth) -
                                        2 * static_cast<int>(character_panel_detail::kPanelPad));
        const int emblemBoxW = character_panel_detail::ClampInt(
            innerWidth * character_panel_detail::kEmblemWidthPct / 100,
            character_panel_detail::kEmblemMinWidth,
            character_panel_detail::kEmblemMaxWidth);
        const int detailsWidth = std::max(0,
                                          innerWidth - emblemBoxW - static_cast<int>(
                                              character_panel_detail::kEmblemGap));
        character_panel_detail::g_stats.name =
                character_panel_detail::FitMiddleEllipsis(
                    snapshot.agent_name,
                    TextSize::Heading,
                    detailsWidth);

        if (snapshot.progress.loaded) {
            const LobbyCharacterProgress& stats = snapshot.progress;
            character_panel_detail::g_stats.level = "LV " + std::to_string(stats.level);
            character_panel_detail::g_stats.wins = std::to_string(stats.wins);
            character_panel_detail::g_stats.losses = std::to_string(stats.losses);
            if (stats.max_level) {
                character_panel_detail::g_stats.xp = "MAX";
            } else {
                const int nextLevelXp = 100 * (static_cast<int>(stats.level) + 1);
                character_panel_detail::g_stats.xp =
                    std::to_string(stats.xptonextlevel) + "/" + std::to_string(nextLevelXp);
            }
            character_panel_detail::g_stats.endurance = std::to_string(stats.endurance);
            character_panel_detail::g_stats.shield = std::to_string(stats.shield);
            character_panel_detail::g_stats.jetpack = std::to_string(stats.jetpack);
            character_panel_detail::g_stats.techslots = std::to_string(stats.techslots);
            character_panel_detail::g_stats.hacking = std::to_string(stats.hacking);
            character_panel_detail::g_stats.contacts = std::to_string(stats.contacts);
        } else {
            character_panel_detail::g_stats.level = "LV 0";
            character_panel_detail::g_stats.wins = "0";
            character_panel_detail::g_stats.losses = "0";
            character_panel_detail::g_stats.xp = "0/100";
            character_panel_detail::g_stats.endurance = "0";
            character_panel_detail::g_stats.shield = "0";
            character_panel_detail::g_stats.jetpack = "0";
            character_panel_detail::g_stats.techslots = "0";
            character_panel_detail::g_stats.hacking = "0";
            character_panel_detail::g_stats.contacts = "0";
        }

        // Fixed label column = widest label, so the three values align in a clean
        // column without stretching label and value apart.
        const int labelColumnWidth = static_cast<int>(
                                         MeasureText(CLAY_STRING("LOSSES"), TextSize::Body).width) + 4;
        int upgradeValueColumnWidth = character_panel_detail::BodyTextWidth(
            character_panel_detail::g_stats.endurance);
        upgradeValueColumnWidth = std::max(
            upgradeValueColumnWidth,
            character_panel_detail::BodyTextWidth(character_panel_detail::g_stats.shield));
        upgradeValueColumnWidth = std::max(
            upgradeValueColumnWidth,
            character_panel_detail::BodyTextWidth(character_panel_detail::g_stats.jetpack));
        upgradeValueColumnWidth = std::max(
            upgradeValueColumnWidth,
            character_panel_detail::BodyTextWidth(character_panel_detail::g_stats.techslots));
        upgradeValueColumnWidth = std::max(
            upgradeValueColumnWidth,
            character_panel_detail::BodyTextWidth(character_panel_detail::g_stats.hacking));
        upgradeValueColumnWidth = std::max(
            upgradeValueColumnWidth,
            character_panel_detail::BodyTextWidth(character_panel_detail::g_stats.contacts));
        upgradeValueColumnWidth = std::max(
            upgradeValueColumnWidth + 4,
            character_panel_detail::kUpgradeValueColumnMinWidth);

        // Crest + details column. The crest spans the identity/stat rows; the
        // action stays in the right column below them, so the card reads as
        // "crest, name/stats, then action" instead of a large avatar poster.
        // The parent LobbyCharacterBox is supplied by the lobby shell; this
        // function emits only content.
        CLAY({ .id = CLAY_ID("CharacterPanelContent"),
             .layout = {
             .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
             .padding = { character_panel_detail::kPanelPad, character_panel_detail::kPanelPad,
             character_panel_detail::kPanelPad, character_panel_detail::kPanelPad },
             .childGap = character_panel_detail::kEmblemGap,
             .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
             .y = CLAY_ALIGN_Y_TOP },
             .layoutDirection = CLAY_LEFT_TO_RIGHT,
             },
             .clip = { .horizontal = true, .vertical = true } }) {
            CLAY({ .id = CLAY_ID("CharacterPanelLeftRail"),
                 .layout = {
                 .sizing = { CLAY_SIZING_FIXED(static_cast<float>(emblemBoxW)),
                 CLAY_SIZING_FIT(0) },
                 .childGap = 4,
                 .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
                 .y = CLAY_ALIGN_Y_TOP },
                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                 },
                 .clip = { .horizontal = true } }) {
                character_panel_detail::AgencyEmblem(a, emblemBoxW);
                character_panel_detail::LevelBadge(
                    character_panel_detail::g_stats.level,
                    emblemBoxW);
            }

            CLAY({ .id = CLAY_ID("CharacterPanelInfoArea"),
                 .layout = {
                 .sizing = { CLAY_SIZING_GROW(0),
                 CLAY_SIZING_GROW(0) },
                .childGap = character_panel_detail::kDetailsGap,
                 .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                 .y = CLAY_ALIGN_Y_TOP },
                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                 },
                 .clip = { .horizontal = true } }) {
                CLAY({ .id = CLAY_ID("CharacterPanelNameHeader"),
                     .layout = {
                     .sizing = { CLAY_SIZING_GROW(0),
                     CLAY_SIZING_FIXED(static_cast<float>(
                         TextLineHeight(TextSize::Heading))) },
                     .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                     .y = CLAY_ALIGN_Y_TOP },
                     },
                     .clip = { .horizontal = true } }) {
                    Text(character_panel_detail::FromStd(character_panel_detail::g_stats.name),
                         TextOpts{
                             .size = TextSize::Heading,
                             .wrap = TextWrap::None,
                             .effect = TextEffect::LegacyPalette(200)
                         });
                }
                CLAY({ .id = CLAY_ID("CharacterPanelDetailsArea"),
                     .layout = {
                     .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW(0) },
                     .childGap = character_panel_detail::kDetailsGap,
                     .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                     .y = CLAY_ALIGN_Y_CENTER },
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     },
                     .clip = { .horizontal = true } }) {
                    CLAY({ .id = CLAY_ID("CharacterPanelStatsColumn"),
                         .layout = {
                         .sizing = { CLAY_SIZING_FIT(0),
                         CLAY_SIZING_GROW(0) },
                         .childGap = character_panel_detail::kInfoGap,
                         .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                         .y = CLAY_ALIGN_Y_TOP },
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         },
                         .clip = { .horizontal = true } }) {
                        CLAY({ .id = CLAY_ID("CharacterPanelStatTable"),
                             .layout = {
                             .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
                             .childGap = character_panel_detail::kStatRowGap,
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             },
                             .clip = { .horizontal = true } }) {
                            character_panel_detail::StatRow(0, "WINS", character_panel_detail::g_stats.wins,
                                                            labelColumnWidth);
                            character_panel_detail::StatRow(1, "LOSSES", character_panel_detail::g_stats.losses,
                                                            labelColumnWidth);
                        }

                        character_panel_detail::XpLine(
                            "XP " + character_panel_detail::g_stats.xp);

                        CLAY({ .id = CLAY_ID("CharacterPanelActionsRow"),
                             .layout = {
                             .sizing = { CLAY_SIZING_GROW(0),
                             CLAY_SIZING_FIXED(character_panel_detail::kButtonHeight) },
                             .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                             .y = CLAY_ALIGN_Y_CENTER },
                             },
                             .clip = { .horizontal = true } }) {
                            Button(CLAY_STRING("CharacterPanelAgentsButton"),
                                   CLAY_STRING("Agents"),
                                   ButtonOpts{
                                       .variant = ButtonVariant::Chrome,
                                       .size = ButtonSize::Auto,
                                       .disabled = state.agentSelectionLocked,
                                       .minWidth = character_panel_detail::kActionButtonMinWidth,
                                       .paddingX = character_panel_detail::kActionButtonPaddingX
                                   },
                                   ButtonHandle{nullptr, character_panel_detail::kActionAgents, &interactions});
                        }
                    }

                    CLAY({ .id = CLAY_ID("CharacterPanelUpgradeWrap"),
                         .layout = {
                         .sizing = { CLAY_SIZING_GROW(0),
                         CLAY_SIZING_GROW(0) },
                         .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
                         .y = CLAY_ALIGN_Y_TOP },
                         },
                         .clip = { .horizontal = true } }) {
                        character_panel_detail::UpgradePanel(
                            upgradeValueColumnWidth);
                    }
                }
            }
        }
    }
} // namespace silencer::client_ui::lobby
