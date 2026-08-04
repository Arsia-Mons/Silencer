#include "ui.h"

#include "client/ui/app_shell/navigation/ui_screen.h"

#include "ui/components/box.h"
#include "ui/components/button.h"
#include "ui/components/input.h"
#include "ui/components/scroll_view.h"
#include "ui/components/text.h"
#include "ui/runtime/element.h"
#include "ui/style/style_patch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace launcher {

using ::ui::AlignItems;
using ::ui::Border;
using ::ui::Color;
using ::ui::EdgeSizes;
using ::ui::FlexDirection;
using ::ui::JustifyContent;
using ::ui::LayoutStyle;
using ::ui::Length;
using ::ui::PositionType;
using ::ui::StyleStatePatch;
using ::ui::TextAlign;
using ::ui::TextVisual;
using ::ui::UiElement;
using ::ui::components::BoxProps;
using ::ui::components::ButtonProps;
using ::ui::components::InputProps;
using ::ui::components::ScrollViewProps;
using ::ui::components::TextProps;

ReactContext LauncherContext = {};

const ViewModel &use_launcher() {
  static const ViewModel empty = {};
  const ViewModel *vm = static_cast<const ViewModel *>(use_context(&LauncherContext));
  return vm ? *vm : empty;
}

// ---- Phosphor-green palette (anchored to web/website/styles.css) ------------
namespace {
constexpr Color kBg = {3, 6, 3, 255};          // #030603 screen
constexpr Color kPanel = {8, 15, 9, 235};      // raised panel
constexpr Color kPanelSoft = {6, 12, 7, 200};  // list row
constexpr Color kBorder = {36, 66, 40, 255};   // sage border
constexpr Color kBorderDim = {20, 40, 24, 255};
constexpr Color kText = {197, 225, 197, 255};   // #c5e1c5
constexpr Color kTextDim = {146, 181, 146, 255};// #92b592
constexpr Color kTextFaint = {91, 138, 101, 255};// #5b8a65
constexpr Color kAccent = {96, 205, 120, 255};  // phosphor highlight
constexpr Color kAccentBright = {209, 250, 215, 255}; // #d1fad7
constexpr Color kAccentDeep = {24, 96, 34, 255};      // filled control
constexpr Color kAccentFaint = {24, 96, 34, 70};      // hover wash
constexpr Color kAccentRow = {17, 45, 24, 255};       // selected row (opaque)
constexpr Color kOnline = {120, 220, 130, 255};
constexpr Color kOffline = {176, 96, 96, 255};
constexpr Color kScrim = {3, 6, 3, 170};

// Glyph faces (baked from the origin sprite banks in bake_glyph_faces):
// Body=0 (bank 133), Large=1 (134), Title=2 (136), Tiny=3 (132), Heading=4
// (135). Text is pixel-perfect ONLY at the face's native line height (or an
// integer multiple): Body 11, Large 15, Title 23, Tiny 7, Heading 19.
constexpr uint16_t kFaceBody = 0;
constexpr uint16_t kFaceTitle = 2;
constexpr uint16_t kFaceHeading = 4;

// Detail-pane text wrap width (Yoga wraps only at a definite points width).
// 900 window − 36 root pad − 32 panel pad − 220 list − 12 gap − 28 detail pad
// − ~22 scrollbar/track.
constexpr float kDetailTextW = 550.0f;
constexpr float kListTextW = 168.0f;

TextVisual tv(Color c, float size, uint16_t face, TextAlign align = TextAlign::Left,
              ::ui::TextWrap wrap = ::ui::TextWrap::None) {
  TextVisual t{};
  t.color = c;
  t.font_id = face;
  t.font_size = size;
  t.align = align;
  t.wrap = wrap;
  return t;
}

StyleStatePatch text_style(Color c, float size, uint16_t face,
                           TextAlign align = TextAlign::Left,
                           ::ui::TextWrap wrap = ::ui::TextWrap::None) {
  return ::ui::patch().text(tv(c, size, face, align, wrap));
}

Border border1(Color c) { return Border{{1, 1, 1, 1}, {c, c, c, c}}; }

StyleStatePatch panel_style(Color bg, Color border, float corner) {
  return ::ui::patch().background(bg).border(border1(border)).corner_radius(corner);
}

const char *dup(const std::string &s) { return ::ui::copy_string(s.c_str()); }

// ---- Primitive wrappers (keyed component instances) ------------------------
UiElement MkBox(const BoxProps &p) { return ::ui::component("Box", p, ::ui::components::Box); }
UiElement MkText(const TextProps &p) { return ::ui::component("Text", p, ::ui::components::Text); }
UiElement MkButton(const ButtonProps &p) {
  return ::ui::component("Button", p, ::ui::components::Button);
}
UiElement MkInput(const InputProps &p) {
  return ::ui::component("Input", p, ::ui::components::Input);
}
UiElement MkScroll(const ScrollViewProps &p) {
  return ::ui::component("ScrollView", p, ::ui::components::ScrollView);
}

UiElement label(const char *key, const std::string &value, Color c, float size,
                uint16_t face, TextAlign align = TextAlign::Left,
                ::ui::TextWrap wrap = ::ui::TextWrap::None, Length width = Length::auto_size()) {
  TextProps p{};
  p.key = key;
  p.value = dup(value);
  p.style = text_style(c, size, face, align, wrap);
  p.layout.width = width;
  return MkText(p);
}

// ---- Style presets ---------------------------------------------------------
StyleStatePatch ghost_button(Color text_color, float size = 11) {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(Color{0, 0, 0, 0}).border(border1(kBorder))
                .corner_radius(3.0f).text(tv(text_color, size, kFaceBody, TextAlign::Center));
  sp.hover = ::ui::patch().background(kAccentFaint).border(border1(kAccent));
  sp.pressed = ::ui::patch().background(Color{16, 40, 20, 200});
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  sp.disabled = ::ui::patch().border(border1(kBorderDim))
                    .text(tv(kTextFaint, size, kFaceBody, TextAlign::Center));
  return sp;
}

// Origin uses bank 135 for button labels (the game's Heading face).
StyleStatePatch accent_button(float size = 19) {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(kAccentDeep).border(border1(kAccent))
                .corner_radius(3.0f).text(tv(kAccentBright, size, kFaceHeading, TextAlign::Center));
  sp.hover = ::ui::patch().background(Color{34, 128, 46, 255}).border(border1(kAccentBright));
  sp.pressed = ::ui::patch().background(Color{20, 72, 28, 255});
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccentBright, 2.0f});
  sp.disabled = ::ui::patch().background(Color{14, 26, 16, 255}).border(border1(kBorderDim))
                    .text(tv(kTextFaint, size, kFaceHeading, TextAlign::Center));
  return sp;
}

StyleStatePatch toggle_button(bool selected) {
  StyleStatePatch sp{};
  if (selected) {
    sp.base = ::ui::patch().background(kAccentDeep).border(border1(kAccent))
                  .corner_radius(3.0f).text(tv(kAccentBright, 11, kFaceBody, TextAlign::Center));
  } else {
    sp.base = ::ui::patch().background(Color{0, 0, 0, 0}).border(border1(kBorderDim))
                  .corner_radius(3.0f).text(tv(kTextDim, 11, kFaceBody, TextAlign::Center));
    sp.hover = ::ui::patch().border(border1(kBorder)).text(tv(kText, 11, kFaceBody, TextAlign::Center));
  }
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  return sp;
}

// Borderless text link (topbar WEBSITE/DISCORD). The zero-width border
// overrides the Button role's default box.
StyleStatePatch link_button() {
  Border none{{0, 0, 0, 0}, {}};
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(Color{0, 0, 0, 0}).border(none)
                .text(tv(kTextFaint, 11, kFaceBody, TextAlign::Center));
  sp.hover = ::ui::patch().text(tv(kAccent, 11, kFaceBody, TextAlign::Center));
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  return sp;
}

StyleStatePatch input_style() {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(kPanelSoft).border(border1(kBorder)).corner_radius(3.0f)
                .text(tv(kText, 11, kFaceBody));
  sp.focus_visible = ::ui::patch().border(border1(kAccent));
  return sp;
}

// ---------------------------------------------------------------------------
LayoutStyle row(float gap = 0) {
  LayoutStyle s{};
  s.direction = FlexDirection::Row;
  s.align_items = AlignItems::Center;
  if (gap)
    s.gap = gap;
  return s;
}

LayoutStyle col(float gap = 0) {
  LayoutStyle s{};
  s.direction = FlexDirection::Column;
  if (gap)
    s.gap = gap;
  return s;
}

UiElement spacer(const char *key) {
  BoxProps p{};
  p.key = key;
  p.layout.flex_grow = 1.0f;
  return MkBox(p);
}

UiElement divider(const char *key) {
  BoxProps p{};
  p.key = key;
  p.layout.width = Length::percent(100.0f);
  p.layout.height = Length::points(1);
  p.style = ::ui::patch().background(kBorderDim);
  return MkBox(p);
}

UiElement dot(const char *key, Color c, bool hollow = false) {
  BoxProps p{};
  p.key = key;
  p.layout.width = Length::points(6);
  p.layout.height = Length::points(6);
  p.style = hollow ? ::ui::patch().background(Color{0, 0, 0, 0}).border(border1(kTextFaint))
                         .corner_radius(3.0f)
                   : ::ui::patch().background(c).corner_radius(3.0f);
  return MkBox(p);
}

// Long filesystem paths don't fit the drop-up; keep the informative tail.
std::string tail_ellipsis(const std::string &s, size_t n) {
  if (s.size() <= n)
    return s;
  return "..." + s.substr(s.size() - (n - 3));
}

UiElement small_button(const char *key, const char *text, Color color,
                       std::function<void(const ::ui::ActivationEvent &)> fn,
                       bool disabled = false) {
  ButtonProps b{};
  b.key = key;
  b.label = text;
  b.disabled = disabled;
  b.style = ghost_button(color, 11);
  b.layout.height = Length::points(24);
  b.layout.padding = EdgeSizes{10, 10, 0, 0};
  b.layout.align_items = AlignItems::Center;
  b.layout.justify_content = JustifyContent::Center;
  b.on_activate = std::move(fn);
  return MkButton(b);
}

// ---- Per-session UI state (held in RootView's fiber) -----------------------
struct UiState {
  int tab = 0; // 0 = news, 1 = releases
  int sel_news = 0;
  int sel_release = 0;
  bool drop_open = false;
  bool auth_open = false;
  bool editing_loc = false;
  std::string loc_text;
  std::string user_text;
  std::string pass_text;
  std::string confirm_uninstall; // channel awaiting confirmation, "" = none
  bool font_qa = false;          // show the font QA panel instead of content
};

// Offscreen self-verify (SILENCER_LAUNCHER_SHOT) can't inject clicks, so
// SILENCER_LAUNCHER_UI_STATE seeds the initial UI state to screenshot the
// overlays: comma-separated tokens "releases", "drop", "auth", "confirm",
// "fontqa".
UiState initial_ui_state() {
  UiState st;
  const char *env = getenv("SILENCER_LAUNCHER_UI_STATE");
  if (!env || !env[0])
    return st;
  std::string s = env;
  if (s.find("releases") != std::string::npos)
    st.tab = 1;
  if (s.find("drop") != std::string::npos)
    st.drop_open = true;
  if (s.find("auth") != std::string::npos)
    st.auth_open = true;
  if (s.find("confirm") != std::string::npos)
    st.confirm_uninstall = "stable";
  if (s.find("fontqa") != std::string::npos)
    st.font_qa = true;
  return st;
}

// ---- News blocks -> elements ------------------------------------------------
std::string flatten(const std::vector<NewsSpan> &spans) {
  std::string out;
  for (const auto &s : spans)
    out += s.text;
  return out;
}

// v1 fidelity note: the Text primitive is one style per node, so inline
// bold/italic render flat for now; links flatten inline and additionally get a
// clickable chip under the paragraph.
void append_blocks(std::vector<UiElement> *out, const std::vector<NewsBlock> &blocks,
                   const Intents &in, const std::string &keybase) {
  for (size_t bi = 0; bi < blocks.size(); ++bi) {
    const NewsBlock &b = blocks[bi];
    const std::string kb = keybase + std::to_string(bi);
    if (b.type == NewsBlock::Type::Heading) {
      out->push_back(label(dup(kb + "-h"), flatten(b.spans), kTextFaint, 11, kFaceBody,
                           TextAlign::Left, ::ui::TextWrap::Words,
                           Length::points(kDetailTextW)));
    } else if (b.type == NewsBlock::Type::Paragraph) {
      out->push_back(label(dup(kb + "-p"), flatten(b.spans), kTextDim, 11, kFaceBody,
                           TextAlign::Left, ::ui::TextWrap::Words,
                           Length::points(kDetailTextW)));
      std::vector<UiElement> chips;
      for (size_t si = 0; si < b.spans.size(); ++si) {
        const NewsSpan &sp = b.spans[si];
        if (sp.href.empty())
          continue;
        std::string url = sp.href;
        // Glyph atlases cover ASCII 33..126 only — no arrow glyphs.
        chips.push_back(small_button(dup(kb + "-l" + std::to_string(si)),
                                     dup(sp.text), kAccent,
                                     [&in, url](const ::ui::ActivationEvent &) {
                                       if (in.open_url)
                                         in.open_url(url);
                                     }));
      }
      if (!chips.empty()) {
        BoxProps chiprow{};
        chiprow.key = dup(kb + "-links");
        chiprow.layout = row(8);
        chiprow.children = ::ui::children(chips);
        out->push_back(MkBox(chiprow));
      }
    } else if (b.type == NewsBlock::Type::List) {
      for (size_t li = 0; li < b.items.size(); ++li) {
        BoxProps item{};
        item.key = dup(kb + "-i" + std::to_string(li));
        item.layout = row(8);
        item.layout.align_items = AlignItems::Start;
        std::string marker = b.ordered ? std::to_string(li + 1) + "." : "-";
        item.children = ::ui::children(
            {label(dup(kb + "-m" + std::to_string(li)), marker, kTextFaint, 11, kFaceBody),
             label(dup(kb + "-t" + std::to_string(li)), flatten(b.items[li]), kTextDim, 11,
                   kFaceBody, TextAlign::Left, ::ui::TextWrap::Words,
                   Length::points(kDetailTextW - 20))});
        out->push_back(MkBox(item));
      }
    }
  }
}

// ---- List/detail scaffolding (shared by NEWS and RELEASES) -----------------
UiElement list_row(const char *key, const std::string &title, const std::string &marker,
                   const std::string &sub, bool selected,
                   std::function<void(const ::ui::ActivationEvent &)> fn) {
  std::vector<UiElement> title_kids;
  title_kids.push_back(label("lr-title", title, selected ? kAccentBright : kText, 11,
                             kFaceBody, TextAlign::Left, ::ui::TextWrap::Words,
                             Length::points(kListTextW)));
  if (!marker.empty()) {
    title_kids.push_back(label("lr-mark", marker, kAccent, 11, kFaceBody));
  }
  BoxProps titlerow{};
  titlerow.key = "lr-tr";
  titlerow.layout = row(6);
  titlerow.layout.align_items = AlignItems::Start;
  titlerow.children = ::ui::children(title_kids);

  BoxProps inner{};
  inner.key = "lr-inner";
  inner.layout = col(2);
  inner.layout.width = Length::percent(100.0f);
  inner.children =
      ::ui::children({MkBox(titlerow), label("lr-sub", sub, kTextFaint, 11, kFaceBody)});

  ButtonProps b{};
  b.key = key;
  b.style = panel_style(selected ? kAccentRow : kPanelSoft, selected ? kAccent : kBorderDim,
                        3.0f);
  b.layout.width = Length::percent(100.0f);
  b.layout.height = Length::auto_size();
  b.layout.align_items = AlignItems::Stretch;
  b.layout.padding = EdgeSizes{10, 10, 8, 6};
  b.on_activate = std::move(fn);
  b.children = ::ui::children({MkBox(inner)});
  return MkButton(b);
}

UiElement list_column(const char *key, std::vector<UiElement> rows) {
  BoxProps inner{};
  inner.key = "list-track";
  inner.layout = col(6);
  inner.layout.width = Length::percent(100.0f);
  inner.children = ::ui::children(rows);

  ScrollViewProps sv{};
  sv.key = "list-scroll";
  sv.id = key;
  sv.layout.width = Length::percent(100.0f);
  sv.layout.flex_grow = 1.0f;
  sv.children = ::ui::children({MkBox(inner)});

  BoxProps colbox{};
  colbox.key = key;
  colbox.layout = col(0);
  colbox.layout.width = Length::points(220);
  colbox.layout.height = Length::percent(100.0f);
  colbox.children = ::ui::children({MkScroll(sv)});
  return MkBox(colbox);
}

UiElement detail_pane(const char *key, std::vector<UiElement> kids) {
  BoxProps inner{};
  inner.key = "detail-track";
  inner.layout = col(8);
  inner.layout.width = Length::percent(100.0f);
  inner.children = ::ui::children(kids);

  ScrollViewProps sv{};
  sv.key = "detail-scroll";
  sv.id = key;
  sv.layout.width = Length::percent(100.0f);
  sv.layout.flex_grow = 1.0f;
  sv.children = ::ui::children({MkBox(inner)});

  BoxProps pane{};
  pane.key = key;
  pane.layout = col(0);
  pane.layout.flex_grow = 1.0f;
  pane.layout.height = Length::percent(100.0f);
  pane.layout.padding = EdgeSizes{14, 14, 12, 12};
  pane.style = panel_style(kPanelSoft, kBorderDim, 3.0f);
  pane.children = ::ui::children({MkScroll(sv)});
  return MkBox(pane);
}

UiElement centered_note(const char *key, const std::string &text, Color c) {
  BoxProps pane{};
  pane.key = key;
  pane.layout = col(0);
  pane.layout.flex_grow = 1.0f;
  pane.layout.align_items = AlignItems::Center;
  pane.layout.justify_content = JustifyContent::Center;
  pane.children = ::ui::children({label("note", text, c, 11, kFaceBody)});
  return MkBox(pane);
}

// ---- NEWS tab ---------------------------------------------------------------
UiElement news_content(const AppSnapshot &s, const Intents &in, UiState *st) {
  if (s.news_status == FetchStatus::Idle || s.news_status == FetchStatus::Loading)
    return centered_note("news-loading", "Loading announcements...", kTextFaint);
  if (s.news_status == FetchStatus::Failed)
    return centered_note("news-failed", "Announcements unavailable", kOffline);
  if (s.announcements.empty())
    return centered_note("news-empty", "No announcements", kTextFaint);

  const int sel = std::min(st->sel_news, (int)s.announcements.size() - 1);

  std::vector<UiElement> rows;
  for (int i = 0; i < (int)s.announcements.size(); ++i) {
    const Announcement &a = s.announcements[i];
    rows.push_back(list_row(dup("news-" + std::to_string(i)), a.title,
                            a.pinned ? "PINNED" : "", a.date, i == sel,
                            [st, i](const ::ui::ActivationEvent &) { st->sel_news = i; }));
  }

  const Announcement &a = s.announcements[sel];
  std::vector<UiElement> detail;
  BoxProps head{};
  head.key = "news-head";
  head.layout = row(10);
  head.layout.width = Length::percent(100.0f);
  head.children = ::ui::children({label("nh-title", a.title, kText, 19, kFaceHeading),
                                  spacer("nh-gap"),
                                  label("nh-date", a.date, kTextFaint, 11, kFaceBody)});
  detail.push_back(MkBox(head));
  detail.push_back(divider("news-div"));
  append_blocks(&detail, a.blocks, in, "nb-" + a.slug + "-");

  BoxProps cols{};
  cols.key = "news-cols";
  cols.layout = row(12);
  cols.layout.align_items = AlignItems::Stretch;
  cols.layout.width = Length::percent(100.0f);
  cols.layout.flex_grow = 1.0f;
  cols.children = ::ui::children(
      {list_column("news-list", std::move(rows)), detail_pane("news-detail", std::move(detail))});
  return MkBox(cols);
}

// ---- RELEASES tab -----------------------------------------------------------
UiElement releases_content(const AppSnapshot &s, UiState *st) {
  if (s.releases_status == FetchStatus::Idle || s.releases_status == FetchStatus::Loading)
    return centered_note("rel-loading", "Loading releases...", kTextFaint);
  if (s.releases_status == FetchStatus::Failed)
    return centered_note("rel-failed", "Releases unavailable", kOffline);

  const bool nightly = s.channel == "nightly";
  const std::vector<Release> &list = nightly ? s.releases_nightly : s.releases_stable;
  if (list.empty())
    return centered_note("rel-empty", "No releases for " + s.channel, kTextFaint);

  const std::string installed = s.active().installed;
  const int sel = std::min(st->sel_release, (int)list.size() - 1);

  std::vector<UiElement> rows;
  for (int i = 0; i < (int)list.size(); ++i) {
    const Release &r = list[i];
    std::string marker = (i == 0) ? "LATEST" : "";
    std::string sub = r.date;
    if (!installed.empty() && r.version == installed)
      sub += " - INSTALLED";
    rows.push_back(list_row(dup("rel-" + std::to_string(i)), "v" + r.version, marker, sub,
                            i == sel,
                            [st, i](const ::ui::ActivationEvent &) { st->sel_release = i; }));
  }

  const Release &r = list[sel];
  std::vector<UiElement> detail;
  BoxProps head{};
  head.key = "rel-head";
  head.layout = row(10);
  head.layout.width = Length::percent(100.0f);
  std::vector<UiElement> head_kids;
  head_kids.push_back(label("rh-ver", "v" + r.version, kText, 19, kFaceHeading));
  head_kids.push_back(label("rh-date", r.date, kTextFaint, 11, kFaceBody));
  head_kids.push_back(spacer("rh-gap"));
  if (!installed.empty() && r.version == installed)
    head_kids.push_back(label("rh-inst", "installed", kTextDim, 11, kFaceBody));
  head.children = ::ui::children(head_kids);
  detail.push_back(MkBox(head));
  detail.push_back(divider("rel-div"));
  if (r.notes.empty()) {
    detail.push_back(label("rel-nonotes", "No notes for this release.", kTextFaint, 11,
                           kFaceBody));
  } else {
    for (size_t i = 0; i < r.notes.size(); ++i) {
      BoxProps item{};
      item.key = dup("rn-" + std::to_string(i));
      item.layout = row(8);
      item.layout.align_items = AlignItems::Start;
      item.children = ::ui::children(
          {label(dup("rnm-" + std::to_string(i)), "-", kTextFaint, 11, kFaceBody),
           label(dup("rnt-" + std::to_string(i)), r.notes[i], kTextDim, 11, kFaceBody,
                 TextAlign::Left, ::ui::TextWrap::Words, Length::points(kDetailTextW - 20))});
      detail.push_back(MkBox(item));
    }
  }

  BoxProps cols{};
  cols.key = "rel-cols";
  cols.layout = row(12);
  cols.layout.align_items = AlignItems::Stretch;
  cols.layout.width = Length::percent(100.0f);
  cols.layout.flex_grow = 1.0f;
  cols.children = ::ui::children(
      {list_column("rel-list", std::move(rows)), detail_pane("rel-detail", std::move(detail))});
  return MkBox(cols);
}

// ---- Font QA specimen (SILENCER_LAUNCHER_UI_STATE=fontqa) -------------------
// Every glyph face at a ramp of sizes, labeled, for style QA.
// "*" marks integer multiples of the face's native line height (pixel-perfect
// 1:1 blits of the origin bank glyphs; everything else nearest-scales).
UiElement font_qa_panel() {
  struct Face {
    const char *bank;
    const char *name;
    uint16_t id;
    int native;
  };
  static const Face faces[] = {
      {"bank 133", "Body", 0, 11},  {"bank 134", "Large", 1, 15},
      {"bank 136", "Title", 2, 23}, {"bank 132", "Tiny", 3, 7},
      {"bank 135", "Heading", 4, 19},
  };
  static const int sizes[] = {7, 11, 14, 15, 19, 22, 23, 28, 33, 38, 46};

  std::vector<UiElement> rows;
  rows.push_back(label("qa-legend", "FONT QA - every face x size ramp. * = integer multiple "
                                    "of the native line height (pixel-perfect).",
                       kTextFaint, 11, kFaceBody));
  rows.push_back(divider("qa-legend-div"));

  for (const Face &f : faces) {
    char head[96];
    snprintf(head, sizeof(head), "%s  (%s, native %dpx)", f.bank, f.name, f.native);
    rows.push_back(label(dup(std::string("qa-head-") + f.name), head, kAccent, 11, kFaceBody));

    for (int sz : sizes) {
      char tag[32];
      snprintf(tag, sizeof(tag), "%s %d%s", f.name, sz, (sz % f.native == 0) ? " *" : "");
      BoxProps r{};
      r.key = dup(std::string("qa-") + f.name + "-" + std::to_string(sz));
      r.layout = row(12);
      r.layout.width = Length::percent(100.0f);
      r.children = ::ui::children(
          {label(dup(std::string("qa-t-") + f.name + std::to_string(sz)), tag, kTextFaint, 11,
                 kFaceBody, TextAlign::Left, ::ui::TextWrap::None, Length::points(90)),
           label(dup(std::string("qa-s-") + f.name + std::to_string(sz)),
                 "SILENCER agents ready 0123456789", kText, (float)sz, f.id)});
      rows.push_back(MkBox(r));
    }
    rows.push_back(divider(dup(std::string("qa-div-") + f.name)));
  }

  BoxProps track{};
  track.key = "qa-track";
  track.layout = col(8);
  track.layout.width = Length::percent(100.0f);
  track.children = ::ui::children(rows);

  ScrollViewProps sv{};
  sv.key = "qa-scroll";
  sv.id = "qa-scroll";
  sv.layout.width = Length::percent(100.0f);
  sv.layout.flex_grow = 1.0f;
  sv.children = ::ui::children({MkBox(track)});

  BoxProps panel{};
  panel.key = "qa-panel";
  panel.layout = col(10);
  panel.layout.width = Length::percent(100.0f);
  panel.layout.flex_grow = 1.0f;
  panel.layout.padding = EdgeSizes{16, 16, 14, 14};
  panel.layout.overflow = ::ui::Overflow::Hidden;
  panel.style = panel_style(kPanel, kBorder, 4.0f);
  panel.children = ::ui::children({MkScroll(sv)});
  return MkBox(panel);
}

// ---- Content panel (tabs) ---------------------------------------------------
UiElement content_panel(const AppSnapshot &s, const Intents &in, UiState *st) {
  auto tab = [&](const char *key, const char *text, int idx) {
    ButtonProps b{};
    b.key = key;
    b.label = text;
    b.style = toggle_button(st->tab == idx);
    b.layout.height = Length::points(24);
    b.layout.padding = EdgeSizes{12, 12, 0, 0};
    b.layout.align_items = AlignItems::Center;
    b.layout.justify_content = JustifyContent::Center;
    b.on_activate = [st, idx](const ::ui::ActivationEvent &) { st->tab = idx; };
    return MkButton(b);
  };

  BoxProps tabs{};
  tabs.key = "tabs";
  tabs.layout = row(8);
  tabs.layout.width = Length::percent(100.0f);
  std::vector<UiElement> tab_kids = {tab("tab-news", "NEWS", 0),
                                     tab("tab-releases", "RELEASES", 1), spacer("tabs-gap")};
  if (st->tab == 1) {
    std::string chlabel = s.channel == "nightly" ? "NIGHTLY" : "STABLE";
    tab_kids.push_back(label("tabs-ch", chlabel, kTextFaint, 11, kFaceBody));
  }
  tabs.children = ::ui::children(tab_kids);

  BoxProps panel{};
  panel.key = "content";
  panel.layout = col(12);
  panel.layout.width = Length::percent(100.0f);
  panel.layout.flex_grow = 1.0f;
  panel.layout.padding = EdgeSizes{16, 16, 14, 14};
  panel.layout.overflow = ::ui::Overflow::Hidden;
  panel.style = panel_style(kPanel, kBorder, 4.0f);
  panel.children = ::ui::children(
      {MkBox(tabs), st->tab == 0 ? news_content(s, in, st) : releases_content(s, st)});
  return MkBox(panel);
}

// ---- Topbar -----------------------------------------------------------------
UiElement top_bar(const AppSnapshot &s, const Intents &in, UiState *st) {
  BoxProps bar{};
  bar.key = "topbar";
  bar.layout = row(16);
  bar.layout.width = Length::percent(100.0f);
  bar.layout.height = Length::points(48);
  bar.layout.padding = EdgeSizes{16, 16, 0, 0};
  bar.style = panel_style(kPanel, kBorder, 4.0f);

  auto link = [&](const char *key, const char *text, const char *url) {
    ButtonProps b{};
    b.key = key;
    b.label = text;
    b.style = link_button();
    b.layout.height = Length::points(24);
    b.layout.align_items = AlignItems::Center;
    b.layout.justify_content = JustifyContent::Center;
    std::string u = url;
    b.on_activate = [&in, u](const ::ui::ActivationEvent &) {
      if (in.open_url)
        in.open_url(u);
    };
    return MkButton(b);
  };

  // Account chip: SIGN IN (out) / USERNAME (in) — toggles the popover.
  ButtonProps acct{};
  acct.key = "acct-chip";
  acct.layout.height = Length::points(26);
  acct.layout.padding = EdgeSizes{10, 10, 0, 0};
  acct.layout.align_items = AlignItems::Center;
  acct.layout.justify_content = JustifyContent::Center;
  acct.layout.gap = 8;
  acct.layout.direction = FlexDirection::Row;
  acct.on_activate = [st](const ::ui::ActivationEvent &) {
    st->auth_open = !st->auth_open;
    st->drop_open = false;
  };
  if (s.auth == AuthStatus::SignedIn) {
    acct.style = ghost_button(kText, 11);
    acct.children = ::ui::children(
        {dot("acct-dot", kOnline), label("acct-name", s.auth_username, kText, 11, kFaceBody)});
  } else if (s.auth == AuthStatus::Connecting) {
    acct.style = ghost_button(kTextFaint, 11);
    acct.label = "SIGNING IN...";
    acct.disabled = true;
  } else {
    acct.style = ghost_button(kAccent, 11);
    acct.label = "SIGN IN";
  }

  UiElement title = label("title", "SILENCER", kText, 23, kFaceTitle);
  bar.children = ::ui::children({title, link("lnk-web", "WEBSITE", "https://arsiamons.com"),
                                 link("lnk-discord", "DISCORD", "https://discord.gg/silencer"),
                                 spacer("tb-gap"), MkButton(acct)});
  return MkBox(bar);
}

// ---- Auth popover (root overlay) --------------------------------------------
UiElement auth_popover(const AppSnapshot &s, const Intents &in, UiState *st) {
  std::vector<UiElement> kids;
  kids.push_back(label("ap-label", "ACCOUNT", kTextFaint, 11, kFaceBody));

  if (s.auth == AuthStatus::SignedIn) {
    BoxProps who{};
    who.key = "ap-who";
    who.layout = row(8);
    who.children = ::ui::children(
        {dot("ap-dot", kOnline), label("ap-name", s.auth_username, kText, 11, kFaceBody)});
    kids.push_back(MkBox(who));
    kids.push_back(label("ap-id", "account #" + std::to_string(s.auth_account_id), kTextFaint,
                         11, kFaceBody));
    kids.push_back(label("ap-note", "The game still asks you to sign in when it starts.",
                         kTextFaint, 11, kFaceBody, TextAlign::Left, ::ui::TextWrap::Words,
                         Length::points(228)));
    kids.push_back(divider("ap-div"));
    kids.push_back(small_button("ap-signout", "SIGN OUT", kTextDim,
                                [&in, st](const ::ui::ActivationEvent &) {
                                  if (in.sign_out)
                                    in.sign_out();
                                  st->auth_open = false;
                                }));
  } else {
    const bool busy = s.auth == AuthStatus::Connecting;
    InputProps user{};
    user.key = "ap-user";
    user.id = "ap-user";
    user.focusable = true;
    user.autofocus = true;
    user.value = dup(st->user_text);
    user.disabled = busy;
    user.style = input_style();
    user.layout.width = Length::points(228);
    user.layout.height = Length::points(28);
    user.layout.padding = EdgeSizes{8, 8, 0, 0};
    user.on_change = [st](const std::string &v) { st->user_text = v.substr(0, 16); };
    kids.push_back(MkInput(user));

    InputProps pass{};
    pass.key = "ap-pass";
    pass.id = "ap-pass";
    pass.focusable = true;
    pass.password = true;
    pass.value = dup(st->pass_text);
    pass.disabled = busy;
    pass.style = input_style();
    pass.layout.width = Length::points(228);
    pass.layout.height = Length::points(28);
    pass.layout.padding = EdgeSizes{8, 8, 0, 0};
    pass.on_change = [st](const std::string &v) { st->pass_text = v.substr(0, 28); };
    kids.push_back(MkInput(pass));

    ButtonProps go{};
    go.key = "ap-go";
    go.label = busy ? "SIGNING IN..." : "SIGN IN / REGISTER";
    go.disabled = busy || st->user_text.empty() || st->pass_text.empty();
    go.style = ghost_button(kAccent, 11);
    go.layout.width = Length::points(228);
    go.layout.height = Length::points(28);
    go.layout.align_items = AlignItems::Center;
    go.layout.justify_content = JustifyContent::Center;
    go.on_activate = [&in, st](const ::ui::ActivationEvent &) {
      if (in.sign_in)
        in.sign_in(st->user_text, st->pass_text);
    };
    kids.push_back(MkButton(go));

    if (s.auth == AuthStatus::Failed && !s.auth_error.empty())
      kids.push_back(label("ap-err", s.auth_error, kOffline, 11, kFaceBody, TextAlign::Left,
                           ::ui::TextWrap::Words, Length::points(228)));
    kids.push_back(label("ap-hint", "New usernames are registered automatically.", kTextFaint,
                         11, kFaceBody, TextAlign::Left, ::ui::TextWrap::Words,
                         Length::points(228)));
  }

  BoxProps pop{};
  pop.key = "auth-pop";
  pop.layout = col(8);
  pop.layout.position = PositionType::Absolute;
  // Root-anchored: below the topbar (18 pad + 48 bar + 6), right-aligned.
  pop.layout.position_inset = EdgeSizes{{}, 30.0f, 72.0f, {}};
  pop.layout.width = Length::points(252);
  pop.layout.padding = EdgeSizes{12, 12, 10, 10};
  pop.style = panel_style(Color{8, 15, 9, 250}, kBorder, 4.0f);
  pop.children = ::ui::children(kids);
  return MkBox(pop);
}

// ---- Channel drop-up (root overlay) -----------------------------------------
std::string channel_status_text(const AppSnapshot &s, const std::string &ch, Color *color) {
  const ChannelState &cs = s.chan(ch);
  *color = kTextDim;
  if (s.update_status != UpdateStatus::Idle && s.update_status != UpdateStatus::Done &&
      s.update_status != UpdateStatus::Failed && s.update_channel == ch) {
    char buf[48];
    snprintf(buf, sizeof(buf), "installing... %d%%", (int)(s.update_progress * 100.0f));
    *color = kAccent;
    return buf;
  }
  if (cs.installed.empty()) {
    if (cs.manifest == ManifestStatus::Unavailable) {
      *color = kOffline;
      return cs.message;
    }
    *color = kTextFaint;
    return "not installed";
  }
  if (!cs.latest_version.empty() && cs.latest_version != cs.installed) {
    *color = kAccent;
    return "v" + cs.installed + " -> v" + cs.latest_version;
  }
  return "v" + cs.installed;
}

UiElement channel_row(const AppSnapshot &s, const Intents &in, UiState *st,
                      const std::string &ch) {
  const ChannelState &cs = s.chan(ch);
  const bool selected = s.channel == ch;
  const bool busy = s.update_status == UpdateStatus::Downloading ||
                    s.update_status == UpdateStatus::Verifying ||
                    s.update_status == UpdateStatus::Extracting;

  Color status_color;
  std::string status = channel_status_text(s, ch, &status_color);

  Color dot_color = kOnline;
  bool hollow = false;
  if (cs.installed.empty())
    hollow = true;
  else if (!cs.latest_version.empty() && cs.latest_version != cs.installed)
    dot_color = kAccent;

  // The select region is its own button; the small actions sit beside it so
  // interactive elements never nest.
  ButtonProps sel{};
  sel.key = dup("chsel-" + ch);
  sel.layout.direction = FlexDirection::Row;
  sel.layout.align_items = AlignItems::Center;
  sel.layout.gap = 8;
  sel.layout.padding = EdgeSizes{6, 6, 0, 0};
  sel.layout.height = Length::points(24);
  StyleStatePatch selstyle{};
  selstyle.base = ::ui::patch().background(Color{0, 0, 0, 0}).border(Border{{0, 0, 0, 0}, {}});
  selstyle.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  sel.style = selstyle;
  sel.on_activate = [&in, ch](const ::ui::ActivationEvent &) {
    if (in.set_channel)
      in.set_channel(ch);
  };
  std::string chname = ch == "nightly" ? "NIGHTLY" : "STABLE";
  sel.children = ::ui::children(
      {dot(dup("chdot-" + ch), dot_color, hollow),
       label(dup("chname-" + ch), chname, selected ? kAccentBright : kText, 11, kFaceBody)});

  // Head: channel select + the actions. Foot: truncated path + status text
  // (the two variable-width strings share the line, ellipsized/short).
  std::vector<UiElement> head_kids = {MkButton(sel), spacer(dup("chgap-" + ch))};
  if (!busy) {
    if (cs.manifest == ManifestStatus::UpdateAvailable || cs.installed.empty()) {
      const bool can = cs.manifest == ManifestStatus::UpdateAvailable;
      head_kids.push_back(small_button(dup("chinst-" + ch),
                                       cs.installed.empty() ? "INSTALL" : "UPDATE", kAccent,
                                       [&in, ch](const ::ui::ActivationEvent &) {
                                         if (in.install)
                                           in.install(ch);
                                       },
                                       !can));
    }
    if (!cs.installed.empty()) {
      head_kids.push_back(small_button(dup("chdel-" + ch), "UNINSTALL", kTextFaint,
                                       [st, ch](const ::ui::ActivationEvent &) {
                                         st->confirm_uninstall = ch;
                                         st->drop_open = false;
                                       }));
    }
  }
  BoxProps head{};
  head.key = dup("chhead-" + ch);
  head.layout = row(8);
  head.layout.width = Length::percent(100.0f);
  head.children = ::ui::children(head_kids);

  BoxProps foot{};
  foot.key = dup("chfoot-" + ch);
  foot.layout = row(8);
  foot.layout.width = Length::percent(100.0f);
  foot.children = ::ui::children(
      {label(dup("chpath-" + ch), tail_ellipsis(s.base_dir + "/" + ch, 32), kTextFaint, 11,
             kFaceBody),
       spacer(dup("chfgap-" + ch)),
       label(dup("chst-" + ch), status, status_color, 11, kFaceBody)});

  BoxProps rowbox{};
  rowbox.key = dup("chrow-" + ch);
  rowbox.layout = col(6);
  rowbox.layout.width = Length::percent(100.0f);
  rowbox.layout.padding = EdgeSizes{12, 12, 8, 8};
  rowbox.layout.overflow = ::ui::Overflow::Hidden;
  rowbox.style = panel_style(selected ? kAccentRow : kPanelSoft,
                             selected ? kAccent : kBorderDim, 3.0f);
  rowbox.children = ::ui::children({MkBox(head), MkBox(foot)});
  return MkBox(rowbox);
}

UiElement channel_dropup(const AppSnapshot &s, const Intents &in, UiState *st) {
  std::vector<UiElement> kids;
  kids.push_back(label("cd-label", "CHANNEL", kTextFaint, 11, kFaceBody));
  kids.push_back(channel_row(s, in, st, "stable"));
  kids.push_back(channel_row(s, in, st, "nightly"));
  kids.push_back(divider("cd-div"));
  kids.push_back(label("cd-loc-label", "INSTALL LOCATION", kTextFaint, 11, kFaceBody));

  BoxProps locrow{};
  locrow.key = "cd-locrow";
  locrow.layout = row(8);
  locrow.layout.width = Length::percent(100.0f);
  if (st->editing_loc) {
    InputProps inp{};
    inp.key = "cd-loc-input";
    inp.id = "cd-loc-input";
    inp.focusable = true;
    inp.autofocus = true;
    inp.value = dup(st->loc_text);
    inp.style = input_style();
    inp.layout.flex_grow = 1.0f;
    inp.layout.height = Length::points(26);
    inp.layout.padding = EdgeSizes{8, 8, 0, 0};
    inp.on_change = [st](const std::string &v) { st->loc_text = v; };
    locrow.children = ::ui::children(
        {MkInput(inp), small_button("cd-loc-save", "SAVE", kAccent,
                                    [&in, st](const ::ui::ActivationEvent &) {
                                      if (in.set_base_dir)
                                        in.set_base_dir(st->loc_text);
                                      st->editing_loc = false;
                                    })});
  } else {
    locrow.children = ::ui::children(
        {label("cd-loc-path", tail_ellipsis(s.base_dir, 36), kTextDim, 11, kFaceBody),
         spacer("cd-loc-gap"),
         small_button("cd-loc-change", "CHANGE...", kTextDim,
                      [st, &s](const ::ui::ActivationEvent &) {
                        st->loc_text = s.base_dir;
                        st->editing_loc = true;
                      })});
  }
  kids.push_back(MkBox(locrow));
  kids.push_back(label("cd-hint", "Each channel installs into its own subfolder.", kTextFaint,
                       11, kFaceBody));

  BoxProps pop{};
  pop.key = "chan-pop";
  pop.layout = col(8);
  pop.layout.position = PositionType::Absolute;
  // Root-anchored: above the playbar (18 pad + 52 bar + 8), right-aligned.
  pop.layout.position_inset = EdgeSizes{{}, 18.0f, {}, 78.0f};
  pop.layout.width = Length::points(400);
  pop.layout.padding = EdgeSizes{12, 12, 10, 10};
  pop.style = panel_style(Color{8, 15, 9, 250}, kBorder, 4.0f);
  pop.children = ::ui::children(kids);
  return MkBox(pop);
}

// ---- Playbar ----------------------------------------------------------------
UiElement update_progress_line(const AppSnapshot &s) {
  std::vector<UiElement> kids;
  if (s.update_status == UpdateStatus::Downloading) {
    char pct[32];
    snprintf(pct, sizeof(pct), "Downloading  %d%%", (int)(s.update_progress * 100.0f));
    BoxProps track{};
    track.key = "pb-track";
    track.layout.width = Length::points(140);
    track.layout.height = Length::points(8);
    track.style = ::ui::patch().background(kBorderDim).corner_radius(4.0f);
    BoxProps fill{};
    fill.key = "pb-fill";
    fill.layout.width = Length::percent(s.update_progress * 100.0f);
    fill.layout.height = Length::percent(100.0f);
    fill.style = ::ui::patch().background(kAccent).corner_radius(4.0f);
    track.children = ::ui::children({MkBox(fill)});
    kids.push_back(label("pb-pct", pct, kTextDim, 11, kFaceBody));
    kids.push_back(MkBox(track));
  } else if (s.update_status == UpdateStatus::Verifying) {
    kids.push_back(label("pb-pct", "Verifying download...", kTextDim, 11, kFaceBody));
  } else if (s.update_status == UpdateStatus::Extracting) {
    kids.push_back(label("pb-pct", "Installing...", kTextDim, 11, kFaceBody));
  } else if (s.update_status == UpdateStatus::Failed && !s.update_error.empty()) {
    kids.push_back(label("pb-pct", s.update_error, kOffline, 11, kFaceBody));
  }
  BoxProps wrap{};
  wrap.key = "pb-progress";
  wrap.layout = row(10);
  wrap.children = ::ui::children(kids);
  return MkBox(wrap);
}

UiElement play_bar(const AppSnapshot &s, const Intents &in, UiState *st) {
  // Ping chip.
  std::string ping_text;
  Color ping_color = kTextFaint;
  Color ping_dot = kTextFaint;
  switch (s.ping) {
  case PingStatus::Probing:
  case PingStatus::Unknown:
    ping_text = "pinging...";
    break;
  case PingStatus::Online:
    ping_text = std::to_string(s.latency_ms) + " ms";
    ping_color = kOnline;
    ping_dot = kOnline;
    break;
  case PingStatus::Offline:
    ping_text = "lobby offline";
    ping_color = kOffline;
    ping_dot = kOffline;
    break;
  }
  BoxProps ping{};
  ping.key = "ping-chip";
  ping.layout = row(8);
  ping.children = ::ui::children({dot("ping-dot", ping_dot, s.ping != PingStatus::Online &&
                                                                s.ping != PingStatus::Offline),
                                  label("ping-ms", ping_text, ping_color, 11, kFaceBody)});

  // Split button: channel segment + primary action.
  const ChannelState &active = s.active();
  const bool busy = s.update_status == UpdateStatus::Downloading ||
                    s.update_status == UpdateStatus::Verifying ||
                    s.update_status == UpdateStatus::Extracting;

  ButtonProps seg{};
  seg.key = "chan-seg";
  seg.layout.direction = FlexDirection::Row;
  seg.layout.align_items = AlignItems::Center;
  seg.layout.gap = 8;
  seg.layout.height = Length::points(52);
  seg.layout.padding = EdgeSizes{14, 14, 0, 0};
  StyleStatePatch segstyle{};
  segstyle.base = ::ui::patch().background(st->drop_open ? kAccentFaint : Color{8, 15, 9, 235})
                      .border(border1(kAccent))
                      .corner_radius(3.0f)
                      .text(tv(kAccentBright, 11, kFaceBody));
  segstyle.hover = ::ui::patch().background(kAccentFaint);
  segstyle.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  seg.style = segstyle;
  seg.on_activate = [st](const ::ui::ActivationEvent &) {
    st->drop_open = !st->drop_open;
    st->auth_open = false;
    st->editing_loc = false;
  };
  std::string seg_text = s.channel == "nightly" ? "NIGHTLY" : "STABLE";
  if (!active.installed.empty())
    seg_text += " - v" + active.installed;
  Color seg_dot = active.installed.empty()
                      ? kTextFaint
                      : (!active.latest_version.empty() && active.latest_version != active.installed)
                            ? kAccent
                            : kOnline;
  seg.children = ::ui::children(
      {dot("seg-dot", seg_dot, active.installed.empty()),
       label("seg-text", seg_text, kAccentBright, 11, kFaceBody),
       // Glyph-atlas coverage is ASCII 33..126: no triangle glyphs, so ^/v.
       label("seg-caret", st->drop_open ? "v" : "^", kTextFaint, 11, kFaceBody)});

  const bool is_installed = !active.installed.empty();
  ButtonProps go{};
  go.key = "primary";
  go.label = is_installed ? "PLAY" : "INSTALL";
  go.style = accent_button();
  go.layout.width = Length::points(180);
  go.layout.height = Length::points(52);
  go.layout.align_items = AlignItems::Center;
  go.layout.justify_content = JustifyContent::Center;
  go.disabled = busy || (is_installed ? !active.binary_valid
                                      : active.manifest != ManifestStatus::UpdateAvailable);
  if (is_installed) {
    go.on_activate = [&in](const ::ui::ActivationEvent &) {
      if (in.play)
        in.play();
    };
  } else {
    std::string ch = s.channel;
    go.on_activate = [&in, ch](const ::ui::ActivationEvent &) {
      if (in.install)
        in.install(ch);
    };
  }

  BoxProps split{};
  split.key = "split";
  split.layout = row(0);
  split.children = ::ui::children({MkButton(seg), MkButton(go)});

  BoxProps bar{};
  bar.key = "playbar";
  bar.layout = row(12);
  bar.layout.width = Length::percent(100.0f);
  bar.layout.height = Length::points(52);
  bar.children = ::ui::children(
      {MkBox(ping), update_progress_line(s), spacer("pb-gap"), MkBox(split)});
  return MkBox(bar);
}

// ---- Uninstall confirm overlay ---------------------------------------------
UiElement confirm_overlay(const AppSnapshot &s, const Intents &in, UiState *st) {
  const std::string ch = st->confirm_uninstall;
  const ChannelState &cs = s.chan(ch);

  std::vector<UiElement> kids;
  kids.push_back(label("cf-label", "UNINSTALL CHANNEL", kTextFaint, 11, kFaceBody));
  std::string what = (ch == "nightly" ? "NIGHTLY" : "STABLE");
  if (!cs.installed.empty())
    what += " v" + cs.installed;
  kids.push_back(label("cf-what", "Remove " + what + " from disk?", kTextDim, 11, kFaceBody));
  kids.push_back(
      label("cf-path", tail_ellipsis(s.base_dir + "/" + ch, 44), kTextFaint, 11, kFaceBody));
  kids.push_back(divider("cf-div"));
  BoxProps btns{};
  btns.key = "cf-btns";
  btns.layout = row(8);
  btns.children = ::ui::children(
      {small_button("cf-yes", "UNINSTALL", kOffline,
                    [&in, st, ch](const ::ui::ActivationEvent &) {
                      if (in.uninstall)
                        in.uninstall(ch);
                      st->confirm_uninstall.clear();
                    }),
       small_button("cf-no", "CANCEL", kTextDim, [st](const ::ui::ActivationEvent &) {
         st->confirm_uninstall.clear();
       })});
  kids.push_back(MkBox(btns));

  BoxProps panel{};
  panel.key = "cf-panel";
  panel.layout = col(8);
  panel.layout.width = Length::points(340);
  panel.layout.padding = EdgeSizes{14, 14, 12, 12};
  panel.style = panel_style(Color{8, 15, 9, 250}, kBorder, 4.0f);
  panel.children = ::ui::children(kids);

  BoxProps overlay{};
  overlay.key = "cf-overlay";
  overlay.layout.position = PositionType::Absolute;
  overlay.layout.position_inset = EdgeSizes::all_edges(::ui::StyleValue::points(0.0f));
  overlay.layout.align_items = AlignItems::Center;
  overlay.layout.justify_content = JustifyContent::Center;
  overlay.style = ::ui::patch().background(kScrim);
  overlay.children = ::ui::children({MkBox(panel)});
  return MkBox(overlay);
}

// Full-window click target that dismisses open popovers: a deliberate dim
// wash (a fully transparent fill trips the solid-fill fast path and erases
// the frame beneath). Painted after the main column but before the popovers,
// so a click anywhere else closes them while the popover stays interactive.
UiElement dismiss_scrim(UiState *st) {
  ButtonProps b{};
  b.key = "scrim";
  b.layout.position = PositionType::Absolute;
  b.layout.position_inset = EdgeSizes::all_edges(::ui::StyleValue::points(0.0f));
  b.layout.width = Length::percent(100.0f);
  b.layout.height = Length::percent(100.0f);
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(Color{3, 6, 3, 110}).border(Border{{0, 0, 0, 0}, {}});
  b.style = sp;
  b.on_activate = [st](const ::ui::ActivationEvent &) {
    st->drop_open = false;
    st->auth_open = false;
    st->editing_loc = false;
  };
  return MkButton(b);
}

// ---- Root ------------------------------------------------------------------
struct RootProps {
  uint32_t unused = 0;
};

UiElement RootView(const RootProps &) {
  const ViewModel &vm = use_launcher();
  static const AppSnapshot empty_snap = {};
  static const Intents empty_intents = {};
  const AppSnapshot &s = vm.snap ? *vm.snap : empty_snap;
  const Intents &in = vm.intents ? *vm.intents : empty_intents;

  UiState *st = use_state<UiState>(initial_ui_state());

  std::vector<UiElement> kids;
  kids.push_back(top_bar(s, in, st));
  kids.push_back(st->font_qa ? font_qa_panel() : content_panel(s, in, st));
  kids.push_back(play_bar(s, in, st));

  if (st->drop_open || st->auth_open)
    kids.push_back(dismiss_scrim(st));
  if (st->auth_open)
    kids.push_back(auth_popover(s, in, st));
  if (st->drop_open)
    kids.push_back(channel_dropup(s, in, st));
  if (!st->confirm_uninstall.empty())
    kids.push_back(confirm_overlay(s, in, st));

  BoxProps rootbox{};
  rootbox.key = "root";
  rootbox.layout = col(14);
  rootbox.layout.width = Length::percent(100.0f);
  rootbox.layout.height = Length::percent(100.0f);
  rootbox.layout.padding = EdgeSizes{18, 18, 18, 18};
  rootbox.style = ::ui::patch().background(kBg);
  if (vm.bg_texture) {
    // Cover-fit the backdrop, dimmed so the phosphor text stays readable;
    // index-0 pixels are transparent and read as the kBg fill beneath.
    ::ui::BackgroundImage img{};
    img.texture_id = vm.bg_texture;
    img.tint = Color{110, 110, 110, 255};
    img.fit = ::ui::ImageFit::Cover;
    rootbox.style = ::ui::patch().background(kBg).image(img);
  }
  rootbox.children = ::ui::children(kids);
  return MkBox(rootbox);
}

} // namespace

// ---- Theme -----------------------------------------------------------------
const ::ui::Theme &launcher_theme() {
  static const ::ui::Theme t = [] {
    ::ui::Theme th{};
    th.text_default = kText;
    th.text_disabled = kTextFaint;
    th.focus_ring = kAccent;
    th.box.base = ::ui::VisualStyle{};
    th.text.base.text = tv(kText, 11, kFaceBody);
    // button/box/input roles are driven entirely by per-instance style patches.
    return th;
  }();
  return t;
}

::ui::UiElement launcher_providers(::ui::UiElement child, const ViewModel *vm) {
  ::ui::UiElement themed = ::ui::provider(
      "LauncherTheme", &::ui::ThemeContext,
      const_cast<::ui::Theme *>(&launcher_theme()), ::ui::children({child}));
  return ::ui::provider("LauncherProvider", &LauncherContext,
                        const_cast<ViewModel *>(vm), ::ui::children({themed}));
}

// ---- Screen ----------------------------------------------------------------
namespace {
class LauncherScreen final : public client::ui::UiScreen {
public:
  const char *debug_name() const override { return "Launcher"; }
  bool wants_transition_fade() const override { return false; }
  bool build_element(::ui::UiElementFrame &, ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component("LauncherRoot", RootProps{}, RootView, "launcher-root");
    return true;
  }
  void build_ui() override {}
};
} // namespace

std::unique_ptr<client::ui::UiScreen> make_launcher_screen() {
  return std::make_unique<LauncherScreen>();
}

} // namespace launcher
