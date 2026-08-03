#include "ui.h"

#include "client/ui/app_shell/navigation/ui_screen.h"

#include "ui/components/box.h"
#include "ui/components/button.h"
#include "ui/components/text.h"
#include "ui/runtime/element.h"
#include "ui/style/style_patch.h"

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
using ::ui::StyleStatePatch;
using ::ui::TextAlign;
using ::ui::TextVisual;
using ::ui::UiElement;
using ::ui::components::BoxProps;
using ::ui::components::ButtonProps;
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

// Font faces (FontRegistry::FaceId): Body=0, Large=1, Title=2, Tiny=3.
constexpr uint16_t kFaceBody = 0;
constexpr uint16_t kFaceLarge = 1;
constexpr uint16_t kFaceTitle = 2;

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

// ---- Button style presets --------------------------------------------------
StyleStatePatch ghost_button(Color text_color) {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(Color{0, 0, 0, 0}).border(border1(kBorder))
                .corner_radius(3.0f).text(tv(text_color, 12, kFaceBody, TextAlign::Center));
  sp.hover = ::ui::patch().background(kAccentFaint).border(border1(kAccent));
  sp.pressed = ::ui::patch().background(Color{16, 40, 20, 200});
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccent, 2.0f});
  sp.disabled = ::ui::patch().border(border1(kBorderDim))
                    .text(tv(kTextFaint, 12, kFaceBody, TextAlign::Center));
  return sp;
}

StyleStatePatch accent_button() {
  StyleStatePatch sp{};
  sp.base = ::ui::patch().background(kAccentDeep).border(border1(kAccent))
                .corner_radius(3.0f).text(tv(kAccentBright, 18, kFaceBody, TextAlign::Center));
  sp.hover = ::ui::patch().background(Color{34, 128, 46, 255}).border(border1(kAccentBright));
  sp.pressed = ::ui::patch().background(Color{20, 72, 28, 255});
  sp.focus_visible = ::ui::patch().outline(::ui::Outline{2.0f, kAccentBright, 2.0f});
  sp.disabled = ::ui::patch().background(Color{14, 26, 16, 255}).border(border1(kBorderDim))
                    .text(tv(kTextFaint, 18, kFaceBody, TextAlign::Center));
  return sp;
}

StyleStatePatch toggle_button(bool selected) {
  StyleStatePatch sp{};
  if (selected) {
    sp.base = ::ui::patch().background(kAccentDeep).border(border1(kAccent))
                  .corner_radius(3.0f).text(tv(kAccentBright, 12, kFaceBody, TextAlign::Center));
  } else {
    sp.base = ::ui::patch().background(Color{0, 0, 0, 0}).border(border1(kBorderDim))
                  .corner_radius(3.0f).text(tv(kTextDim, 12, kFaceBody, TextAlign::Center));
    sp.hover = ::ui::patch().border(border1(kBorder)).text(tv(kText, 12, kFaceBody, TextAlign::Center));
  }
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

// ---- Channel toggle --------------------------------------------------------
UiElement channel_toggle(const AppSnapshot &s, const Intents &in) {
  auto make = [&](const char *key, const char *labeltext, const char *value) {
    ButtonProps b{};
    b.key = key;
    b.label = labeltext;
    b.style = toggle_button(s.channel == value);
    b.layout.width = Length::points(96);
    b.layout.height = Length::points(30);
    b.layout.align_items = AlignItems::Center;
    b.layout.justify_content = JustifyContent::Center;
    std::string v = value;
    b.on_activate = [&in, v](const ::ui::ActivationEvent &) {
      if (in.set_channel)
        in.set_channel(v);
    };
    return MkButton(b);
  };
  BoxProps wrap{};
  wrap.key = "channels";
  wrap.layout = row(8);
  wrap.children = ::ui::children({make("ch-stable", "STABLE", "stable"),
                                  make("ch-nightly", "NIGHTLY", "nightly")});
  return MkBox(wrap);
}

// ---- Update status / progress ---------------------------------------------
UiElement update_status(const AppSnapshot &s, const Intents &in) {
  std::vector<UiElement> kids;

  // An in-flight or finished update overrides the manifest status line.
  if (s.update_status == UpdateStatus::Downloading) {
    char pct[32];
    snprintf(pct, sizeof(pct), "Downloading  %d%%", (int)(s.update_progress * 100.0f));
    BoxProps track{};
    track.key = "track";
    track.layout.width = Length::points(180);
    track.layout.height = Length::points(8);
    track.style = ::ui::patch().background(kBorderDim).corner_radius(4.0f);
    BoxProps fill{};
    fill.key = "fill";
    fill.layout.width = Length::percent(s.update_progress * 100.0f);
    fill.layout.height = Length::percent(100.0f);
    fill.style = ::ui::patch().background(kAccent).corner_radius(4.0f);
    track.children = ::ui::children({MkBox(fill)});
    kids.push_back(label("us", pct, kTextDim, 12, kFaceBody));
    kids.push_back(MkBox(track));
  } else if (s.update_status == UpdateStatus::Verifying) {
    kids.push_back(label("us", "Verifying download…", kTextDim, 12, kFaceBody));
  } else if (s.update_status == UpdateStatus::Extracting) {
    kids.push_back(label("us", "Installing update…", kTextDim, 12, kFaceBody));
  } else if (s.update_status == UpdateStatus::Failed) {
    kids.push_back(label("us", "Update failed: " + s.update_error, kOffline, 12, kFaceBody));
  } else {
    switch (s.manifest_status) {
    case ManifestStatus::Idle:
    case ManifestStatus::Loading:
      kids.push_back(label("us", "Checking for updates…", kTextFaint, 12, kFaceBody));
      break;
    case ManifestStatus::UpToDate: {
      std::string v = s.installed_version.empty() ? s.manifest_version : s.installed_version;
      kids.push_back(label("us", "Up to date · v" + v, kTextDim, 12, kFaceBody));
      break;
    }
    case ManifestStatus::Unavailable:
      kids.push_back(label("us", s.manifest_message, kOffline, 12, kFaceBody));
      break;
    case ManifestStatus::UpdateAvailable: {
      kids.push_back(
          label("us", "Update available: v" + s.manifest_version, kAccent, 12, kFaceBody));
      ButtonProps b{};
      b.key = "do-update";
      b.label = "UPDATE";
      b.style = ghost_button(kAccent);
      b.layout.width = Length::points(96);
      b.layout.height = Length::points(30);
      b.layout.align_items = AlignItems::Center;
      b.layout.justify_content = JustifyContent::Center;
      b.on_activate = [&in](const ::ui::ActivationEvent &) {
        if (in.start_update)
          in.start_update();
      };
      kids.push_back(MkButton(b));
      break;
    }
    }
  }

  BoxProps wrap{};
  wrap.key = "update";
  wrap.layout = row(12);
  wrap.children = ::ui::children(kids);
  return MkBox(wrap);
}

UiElement top_bar(const AppSnapshot &s, const Intents &in) {
  BoxProps bar{};
  bar.key = "topbar";
  bar.layout = row(16);
  bar.layout.width = Length::percent(100.0f);
  bar.layout.height = Length::points(48);
  bar.layout.padding = EdgeSizes{16, 16, 0, 0};
  bar.style = panel_style(kPanel, kBorder, 4.0f);

  UiElement title = label("title", "SILENCER", kText, 22, kFaceTitle);
  BoxProps right{};
  right.key = "tb-right";
  right.layout = row(18);
  right.children = ::ui::children({update_status(s, in), channel_toggle(s, in)});

  bar.children = ::ui::children({title, spacer("tb-gap"), MkBox(right)});
  return MkBox(bar);
}

// ---- Server list -----------------------------------------------------------
UiElement server_row(const AppSnapshot &s, const Intents &in, int i) {
  const ServerView &sv = s.servers[i];
  const bool selected = (i == s.selected_server);

  std::string latency;
  Color lat_color = kTextFaint;
  switch (sv.ping) {
  case PingStatus::Probing: latency = "pinging…"; break;
  case PingStatus::Online:
    latency = std::to_string(sv.latency_ms) + " ms";
    lat_color = kOnline;
    break;
  case PingStatus::Offline: latency = "offline"; lat_color = kOffline; break;
  case PingStatus::Unknown: latency = ""; break;
  }

  BoxProps titlerow{};
  titlerow.key = "srow-title";
  titlerow.layout = row(8);
  titlerow.layout.width = Length::percent(100.0f);
  titlerow.children = ::ui::children({
      label("srow-name", sv.name, selected ? kAccentBright : kText, 16, kFaceBody),
      spacer("srow-gap"),
      label("srow-lat", latency, lat_color, 12, kFaceBody, TextAlign::Right),
  });

  char hostport[256];
  snprintf(hostport, sizeof(hostport), "%s:%d", sv.host.c_str(), sv.port);

  BoxProps inner{};
  inner.key = "srow-inner";
  inner.layout = col(2);
  inner.layout.width = Length::percent(100.0f);
  inner.children = ::ui::children({MkBox(titlerow),
                                   label("srow-host", hostport, kTextFaint, 11, kFaceBody)});

  ButtonProps b{};
  b.key = dup("srv-" + std::to_string(i));
  b.style = panel_style(selected ? kAccentRow : kPanelSoft,
                        selected ? kAccent : kBorderDim, 3.0f);
  b.layout.width = Length::percent(100.0f);
  b.layout.height = Length::auto_size();
  b.layout.align_items = AlignItems::Stretch;
  b.layout.justify_content = JustifyContent::Center;
  b.layout.padding = EdgeSizes{12, 12, 8, 8};
  b.on_activate = [&in, i](const ::ui::ActivationEvent &) {
    if (in.select_server)
      in.select_server(i);
  };
  b.children = ::ui::children({MkBox(inner)});
  return MkButton(b);
}

UiElement play_button(const AppSnapshot &s, const Intents &in) {
  ButtonProps b{};
  b.key = "play";
  b.label = s.game_binary_valid ? "PLAY" : "GAME NOT FOUND";
  b.disabled = !s.game_binary_valid;
  b.style = accent_button();
  b.layout.width = Length::percent(100.0f);
  b.layout.height = Length::points(52);
  b.layout.align_items = AlignItems::Center;
  b.layout.justify_content = JustifyContent::Center;
  b.on_activate = [&in](const ::ui::ActivationEvent &) {
    if (in.play)
      in.play();
  };
  return MkButton(b);
}

UiElement server_panel(const AppSnapshot &s, const Intents &in) {
  std::vector<UiElement> rows;
  rows.push_back(label("srv-label", "SERVERS", kTextFaint, 12, kFaceBody));
  for (int i = 0; i < (int)s.servers.size(); ++i)
    rows.push_back(server_row(s, in, i));
  if (s.servers.empty())
    rows.push_back(label("srv-empty", "No servers configured", kTextFaint, 12, kFaceBody));

  BoxProps list{};
  list.key = "srv-list";
  list.layout = col(8);
  list.layout.width = Length::percent(100.0f);
  list.children = ::ui::children(rows);

  BoxProps panel{};
  panel.key = "server-col";
  panel.layout = col(14);
  panel.layout.width = Length::points(280);
  panel.layout.height = Length::percent(100.0f);
  panel.layout.padding = EdgeSizes{16, 16, 16, 16};
  panel.style = panel_style(kPanel, kBorder, 4.0f);
  panel.children = ::ui::children({MkBox(list), spacer("srv-spacer"), play_button(s, in)});
  return MkBox(panel);
}

// ---- News ------------------------------------------------------------------
UiElement news_item(const Announcement &a, int i) {
  std::vector<UiElement> kids;
  BoxProps titlerow{};
  titlerow.key = "ni-title";
  titlerow.layout = row(8);
  titlerow.layout.width = Length::percent(100.0f);
  std::vector<UiElement> tr;
  if (a.pinned)
    tr.push_back(label("ni-pin", "PINNED", kAccent, 10, kFaceBody));
  tr.push_back(label("ni-name", a.title, kText, 16, kFaceBody));
  tr.push_back(spacer("ni-gap"));
  if (!a.date.empty())
    tr.push_back(label("ni-date", a.date, kTextFaint, 11, kFaceBody, TextAlign::Right));
  titlerow.children = ::ui::children(tr);
  kids.push_back(MkBox(titlerow));

  if (!a.body.empty()) {
    std::string body = a.body;
    if (body.size() > 220)
      body = body.substr(0, 217) + "…";
    // Text wraps only at a definite points width (Yoga measures the text node
    // against it). 504 = news-panel content width at the fixed 900px window.
    kids.push_back(label("ni-body", body, kTextDim, 12, kFaceBody, TextAlign::Left,
                         ::ui::TextWrap::Words, Length::points(504.0f)));
  }

  BoxProps item{};
  item.key = dup("news-" + std::to_string(i));
  item.layout = col(6);
  item.layout.width = Length::percent(100.0f);
  item.layout.padding = EdgeSizes{14, 14, 12, 12};
  item.style = panel_style(kPanelSoft, kBorderDim, 3.0f);
  item.children = ::ui::children(kids);
  return MkBox(item);
}

UiElement news_panel(const AppSnapshot &s) {
  std::vector<UiElement> kids;
  kids.push_back(label("news-label", "NEWS", kTextFaint, 12, kFaceBody));

  if (s.news_status == NewsStatus::Loading) {
    kids.push_back(label("news-loading", "Loading announcements…", kTextFaint, 12, kFaceBody));
  } else if (s.news_status == NewsStatus::Loaded && !s.announcements.empty()) {
    const int cap = 6;
    for (int i = 0; i < (int)s.announcements.size() && i < cap; ++i)
      kids.push_back(news_item(s.announcements[i], i));
  } else {
    kids.push_back(label("news-empty", "No announcements", kTextFaint, 16, kFaceBody));
  }

  BoxProps panel{};
  panel.key = "news-col";
  panel.layout = col(12);
  panel.layout.flex_grow = 1.0f;
  panel.layout.height = Length::percent(100.0f);
  panel.layout.padding = EdgeSizes{18, 18, 16, 16};
  panel.layout.overflow = ::ui::Overflow::Hidden;
  panel.style = panel_style(kPanel, kBorder, 4.0f);
  panel.children = ::ui::children(kids);
  return MkBox(panel);
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

  BoxProps body{};
  body.key = "body";
  body.layout = row(16);
  body.layout.width = Length::percent(100.0f);
  body.layout.flex_grow = 1.0f;
  body.children = ::ui::children({server_panel(s, in), news_panel(s)});

  BoxProps rootbox{};
  rootbox.key = "root";
  rootbox.layout = col(16);
  rootbox.layout.width = Length::percent(100.0f);
  rootbox.layout.height = Length::percent(100.0f);
  rootbox.layout.padding = EdgeSizes{18, 18, 18, 18};
  rootbox.style = ::ui::patch().background(kBg);
  rootbox.children = ::ui::children({top_bar(s, in), MkBox(body)});
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
    th.text.base.text = tv(kText, 12, kFaceBody);
    // button/box roles are driven entirely by per-instance style patches.
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
