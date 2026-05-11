# Clay UI: Idiomatic Patterns

Research on what *good* / *canonical* Clay UI usage looks like, based on
Nic Barker's official Clay repository
([nicbarker/clay](https://github.com/nicbarker/clay)), the README, the
official website demo (`examples/clay-official-website/`), the SDL3 and
raylib demos, talks, and external commentary (Simon Willison, HN). All
references are listed at the bottom.

---

## TL;DR

- **Yes, idiomatic Clay is built by composing many small functions** —
  `Button(...)`, `SidebarItem(...)`, `LandingPageBlob(...)` — exactly the
  way React/SwiftUI/Flutter encourage. Functions emit `CLAY(...) { ... }`
  blocks; calling the function is the equivalent of "rendering" the
  component. There are no classes, no children-as-arg, no JSX — just C
  functions that produce side effects into Clay's frame buffer.
- **Responsive layout is expressed with `CLAY_SIZING_GROW / FIT / FIXED /
  PERCENT` on a flexbox-like model**, not with breakpoints or media
  queries. For *coarse* responsiveness (mobile vs desktop), the canonical
  pattern is a top-level `if (isMobileScreen)` that swaps in an entirely
  different component subtree (`LandingPageDesktop()` vs
  `LandingPageMobile()`). Clay has no built-in breakpoint system; you
  branch yourself.
- **Reusable "style structs" are idiomatic.** Declaring a
  `Clay_ElementDeclaration` at file scope and passing it as the second
  arg to `CLAY(...)` is explicitly endorsed by the README — this is how
  experienced Clay code avoids retyping `.padding` / `.backgroundColor` /
  `.cornerRadius` everywhere.
- **Interaction is immediate-mode and stateless in Clay itself.**
  `Clay_Hovered()` is called *inside* an element's body and returns the
  current hover state. `Clay_OnHover(callback, userData)` attaches a
  callback that fires inside the layout pass. Any persistent state
  (open/closed, scroll position when you want it externally, selected
  tab) must live in *your* app state, not Clay — Clay rebuilds the tree
  every frame.
- **The big anti-pattern is unstable IDs**: using `CLAY_AUTO_ID` for
  things you'll later target (floating containers, transitions, retained
  backends) silently breaks when the hierarchy reorders. Use
  `CLAY_ID("Name")` for named/queryable elements and `CLAY_IDI("Item",
  i)` (or `CLAY_IDI_LOCAL`) for loops.

The single most surprising thing: **Clay is *layout only*** — it emits
`Clay_RenderCommand`s and you supply a renderer. There is no built-in
text input, no clipboard, no focus system, no keyboard event routing, no
widget library. "Composing components" therefore means composing
*layout* and *visual chrome*, not behavior. Behavior is plain old C
state that you mutate from your callbacks and read back next frame.

---

## Idiomatic composition style

### The macro shape

A Clay element is a block, not an expression:

```c
CLAY(CLAY_ID("OuterContainer"),
     { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                   .padding = CLAY_PADDING_ALL(16),
                   .childGap = 16 },
       .backgroundColor = { 250, 250, 255, 255 } }) {
    // children go here
}
```

Source: [README, Quick Start](https://github.com/nicbarker/clay/blob/main/README.md).
The macro takes an `id` and a designated-initialised
`Clay_ElementDeclaration`. The trailing `{ ... }` block is the children
scope — you write nested `CLAY(...) { ... }` and `CLAY_TEXT(...)` calls
inside, plus normal C control flow (`if`, `for`, function calls).

`CLAY_TEXT` is a leaf — no body block — and takes a `Clay_String` plus a
`Clay_TextElementConfig`:

```c
CLAY_TEXT(CLAY_STRING("John Smith"),
          { .fontId = FONT_ID_LATO, .fontSize = 24,
            .textColor = { 255, 0, 0, 255 } });
```

### Components are just C functions

The README says it plainly: "Re-usable 'components' are just functions
that declare more UI."

The canonical README example
([nicbarker/clay README](https://github.com/nicbarker/clay/blob/main/README.md)):

```c
void SidebarItemComponent() {
    CLAY(CLAY_AUTO_ID(), sidebarItemConfig) {
        // children go here...
    }
}

// usage
for (int i = 0; i < 5; i++) {
    SidebarItemComponent();
}
```

And:

```c
void ButtonComponent(Clay_String buttonText) {
    CLAY_AUTO_ID({ .layout = { .padding = CLAY_PADDING_ALL(8) },
                   .backgroundColor = COLOR_RED }) {
        CLAY_TEXT(buttonText, textConfig);
    }
}

CLAY(CLAY_ID("parent"),
     { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
    for (int i = 0; i < textArray.length; i++) {
        CLAY_TEXT(textArray.elements[i], textConfig);
    }
    if (isMobileScreen) {
        CLAY(0) { /* etc */ }
    }
    ButtonComponent(CLAY_STRING("Click me!"));
    ButtonComponent(CLAY_STRING("No, click me!"));
}
```

Note three things React-ish but C-flavoured:

1. **Components are void functions taking value parameters** (no
   "props" struct unless you make one). Anything dynamic — text,
   colour, index — is a function arg.
2. **They use `CLAY_AUTO_ID()` for the root element** so the same
   component can be called many times without ID collisions. (Caveat:
   see anti-patterns below.)
3. **Children are not passed**: a component's body is fixed at the call
   site. There is no equivalent of `<Card>{children}</Card>`. If you
   want "open-ended" children you write your container as a `CLAY(...)
   { ... }` block inline at the call site, optionally wrapping that in
   a helper function for the *outer chrome* only. Some codebases work
   around this with a function-returning-config style (see below).

### Style structs as the "props" pattern

The README endorses splitting style from structure:

```c
Clay_ElementDeclaration reuseableStyle = (Clay_ElementDeclaration) {
    .layout = { .padding = { .left = 12 } },
    .backgroundColor = { 120, 120, 120, 255 },
    .cornerRadius = { 12, 12, 12, 12 }
};

CLAY(CLAY_ID("box"), reuseableStyle) {
    // ...
}
```

Source:
[README, Layout configuration](https://github.com/nicbarker/clay/blob/main/README.md).
This is the closest thing to a Tailwind class or a SwiftUI `ViewModifier`
in idiomatic Clay. The raylib sidebar example takes it a step further by
making the style itself a *function of state*:

```c
// examples/raylib-sidebar-scrolling-container/main.c
Clay_ElementDeclaration HeaderButtonStyle(bool hovered) {
    return (Clay_ElementDeclaration) {
        .layout = { .padding = { 16, 16, 8, 8 } },
        .backgroundColor = hovered ? COLOR_ORANGE : COLOR_BLUE,
    };
}

void RenderHeaderButton(Clay_String text) {
    CLAY_AUTO_ID(HeaderButtonStyle(Clay_Hovered())) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG(headerTextConfig));
    }
}
```

Source:
[examples/raylib-sidebar-scrolling-container/main.c](https://github.com/nicbarker/clay/blob/main/examples/raylib-sidebar-scrolling-container/main.c).
This is the most common "hover-aware button" idiom: a style function
that takes a `hovered` bool, and a render function that calls
`Clay_Hovered()` and passes it through.

### Real-world component decomposition (official website)

The Clay website
([examples/clay-official-website/main.c](https://github.com/nicbarker/clay/blob/main/examples/clay-official-website/main.c))
is the single best canonical reference. It decomposes into ~15 named
functions:

- `LandingPageBlob(int index, int fontSize, Clay_Color color, Clay_String text, Clay_String imageURL)`
- `LandingPageDesktop()` / `LandingPageMobile()`
- `FeatureBlocksDesktop()` / `FeatureBlocksMobile()`
- `DeclarativeSyntaxPageDesktop()` / `DeclarativeSyntaxPageMobile()`
- `HighPerformancePageDesktop()` / `HighPerformancePageMobile()`
- `RendererPageDesktop()` / `RendererPageMobile()`
- `DebuggerPageDesktop()`
- `RendererButtonActive(...)` / `RendererButtonInactive(...)`
- `CreateLayout(bool isMobileScreen, float lerpValue)`

A representative reusable atom:

```c
void LandingPageBlob(int index, int fontSize, Clay_Color color,
                    Clay_String text, Clay_String imageURL) {
    CLAY(CLAY_IDI("HeroBlob", index),
         { .layout = { .sizing = { CLAY_SIZING_GROW(.max = 480) },
                       .padding = CLAY_PADDING_ALL(16),
                       .childGap = 16,
                       .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
           .border = { .color = color, .width = { 2, 2, 2, 2 } },
           .cornerRadius = CLAY_CORNER_RADIUS(10) }) {
        CLAY(CLAY_IDI("CheckImage", index),
             { .layout = { .sizing = { CLAY_SIZING_FIXED(32) } },
               .aspectRatio = { 1 },
               .image = { .imageData = FrameAllocateString(imageURL) } }) {}
        CLAY_TEXT(text,
                  CLAY_TEXT_CONFIG({ .fontSize = fontSize,
                                     .fontId = FONT_ID_BODY_24,
                                     .textColor = color }));
    }
}
```

Source:
[examples/clay-official-website/main.c](https://github.com/nicbarker/clay/blob/main/examples/clay-official-website/main.c).

Notice: it's a function, it takes parameters (including the `index`
used to generate a stable `CLAY_IDI` ID), and it emits a self-contained
element subtree. This is the unit of composition.

### When *not* to write a function

The video-demo shared layout
([examples/shared-layouts/clay-video-demo.c](https://github.com/nicbarker/clay/blob/main/examples/shared-layouts/clay-video-demo.c))
defines only two helper functions (`RenderHeaderButton`,
`RenderDropdownMenuItem`) and inlines the rest of the layout in
`ClayVideoDemo_CreateLayout()` as one large nested block. The rule of
thumb seems to be: **extract a function when you call it more than once
or when it makes the parent more readable, not because Clay requires
it**. Plenty of canonical code is one fat nested block per "screen".

---

## Layout primitives

### Sizing

Clay's flex-style sizing axes are the responsive primitives:

- `CLAY_SIZING_FIXED(value)` — exact pixels.
- `CLAY_SIZING_FIT(.min = a, .max = b)` — wrap children, clamped.
- `CLAY_SIZING_GROW(.min = a, .max = b)` — fill remaining space along
  parent's main axis, clamped. `CLAY_SIZING_GROW(0)` is the common
  "just grow" shorthand.
- `CLAY_SIZING_PERCENT(0.5f)` — percentage of parent.

Source:
[README, Layout configuration](https://github.com/nicbarker/clay/blob/main/README.md).

### Direction and alignment

```c
.layout = {
    .layoutDirection = CLAY_TOP_TO_BOTTOM,   // or CLAY_LEFT_TO_RIGHT
    .padding = CLAY_PADDING_ALL(16),
    .childGap = 16,
    .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
                        .y = CLAY_ALIGN_Y_CENTER }
}
```

That's the whole primitive set: `Row` and `Column` are not types in
Clay, they're `.layoutDirection = CLAY_LEFT_TO_RIGHT` vs
`CLAY_TOP_TO_BOTTOM`. Idiomatic code does not wrap these in `Row(...)`
and `Column(...)` helper functions — the inline designated initialiser
is already terse and readers expect it. (You *could* write them, but
none of the official examples do.)

### Responsive design

There is no `@media` equivalent. The canonical pattern (from the website
demo) is to compute a boolean and branch:

```c
bool isMobileScreen = windowWidth < 750;
if (debugModeEnabled) { isMobileScreen = windowWidth < 950; }
return CreateLayout(isMobileScreen, animationLerpValue);
```

Inside `CreateLayout` you call either `LandingPageDesktop()` or
`LandingPageMobile()`. These are *separate* component functions, not one
component with a responsive flag — the differences (horizontal split
becoming vertical stack, padding changes, sizing changes from
`CLAY_SIZING_PERCENT(0.5f)` to `CLAY_SIZING_GROW(0)`) are big enough
that one parameterised function would be more confusing.

For *fine-grained* responsiveness (a card that fits text up to 480px
then wraps), use `CLAY_SIZING_GROW(.max = 480)` on the child — that's
real intrinsic responsiveness, no branching needed.

### Spacers

There's no `Spacer` element. The flex model handles the same use cases
with `childAlignment` + `CLAY_SIZING_GROW`. If you really want one,
people emit an empty `CLAY(0) { }` with `.layout.sizing.width =
CLAY_SIZING_GROW(0)` — but I didn't see this in canonical examples
because alignment usually suffices.

### Floating / overlay elements

For tooltips, dropdowns, popovers, etc., Clay has a separate `.floating`
config that decouples an element's *layout position* from its
*declaration site*:

```c
CLAY(CLAY_ID("OptionTooltip"),
     { .floating = { .parentId = CLAY_IDI("Option", 2).id,
                     .zIndex = 1,
                     .attachPoints = {
                         .element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                         .parent  = CLAY_ATTACH_POINT_TOP_CENTER } } }) {
    CLAY_TEXT(CLAY_STRING("Most popular!"), {});
}
```

Source: [README, Floating Elements with Attach Points](https://github.com/nicbarker/clay/blob/main/README.md).
This is how you'd build menus and dropdowns — you reference the trigger
element's ID, and Clay positions the floating element next to it. Note
the dependency on stable IDs.

---

## Interaction patterns

Clay's interaction model is fully immediate-mode and exposes three
primitives:

### 1. `Clay_Hovered()` — inline hover state

Call inside an open element block; returns `bool`:

```c
CLAY(CLAY_ID("Button"),
     { .backgroundColor = Clay_Hovered() ? COLOR_BLUE : COLOR_ORANGE }) {
    bool buttonHovered = Clay_Hovered();
    CLAY_TEXT(buttonHovered ? CLAY_STRING("Hovered")
                            : CLAY_STRING("Hover me!"),
              headerTextConfig);
}
```

Source: [README, Hover State](https://github.com/nicbarker/clay/blob/main/README.md).

This is the most common pattern. The `.backgroundColor` field accepts a
ternary because the whole declaration is rebuilt each frame — there is
no "state" being mutated, just a value being computed.

### 2. `Clay_OnHover(callback, userData)` — click/press callback

```c
void HandleButtonInteraction(Clay_ElementId elementId,
                             Clay_PointerData pointerInfo,
                             void *userData) {
    ButtonData *buttonData = (ButtonData *)userData;
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        NavigateTo(buttonData->link);
    }
}

CLAY(CLAY_ID("Button"),
     { .layout = { .padding = CLAY_PADDING_ALL(8) } }) {
    Clay_OnHover(HandleButtonInteraction, &linkButton);
    CLAY_TEXT(CLAY_STRING("Button"), &headerTextConfig);
}
```

Source: [README, Pointer Interaction](https://github.com/nicbarker/clay/blob/main/README.md).
Pass a function pointer and opaque `userData`. The callback fires
inside the layout pass on the frame the pointer is over the element
*and* you can check `pointerInfo.state` for press/release semantics.

### 3. `Clay_PointerOver(id)` — out-of-band query

```c
if (mouseButtonDown(0)
    && Clay_PointerOver(Clay_GetElementId("ProfilePicture"))) {
    // Handle profile picture clicked
}
```

Source: [README, Pointer Overlap Queries](https://github.com/nicbarker/clay/blob/main/README.md).
Use this when you want to check hover/click from *outside* the layout
function — e.g., in your input-handling code before `Clay_BeginLayout()`.

### Where does state live?

**Not in Clay.** Clay rebuilds every frame; it remembers nothing about
your app. Open/closed menus, selected tabs, drag offsets, focus, text
field contents — all of these are plain C variables in your app code,
read at layout time and mutated by callbacks or input handling.

The raylib sidebar example shows the manual scrollbar pattern: store a
`scrollbarData` struct in your app state, mutate it from your input
loop, and read its `mouseDown` / `clickOrigin` / `positionOrigin`
fields inside the layout function to position the scrollbar thumb.
([raylib-sidebar-scrolling-container/main.c](https://github.com/nicbarker/clay/blob/main/examples/raylib-sidebar-scrolling-container/main.c).)

### One frame delay caveat

From the README: "The bounding box queried by `Clay_PointerOver` is
from the last frame. This shouldn't make a difference except in the
case of animations that move at high speed. If this is an issue for
you, performing layout twice per frame with the same data will give
you the correct interaction the second time."

This is the classic immediate-mode hover-latency issue. For mostly
static UIs it doesn't matter; for fast-moving things it does, and the
fix is literally to run the layout pass twice.

---

## Anti-patterns and gotchas

### 1. Unstable IDs from `CLAY_AUTO_ID` in the wrong places

`CLAY_AUTO_ID()` generates IDs from hierarchy position. The README
explicitly warns: IDs "may change between layout calls if elements are
added / removed from the hierarchy before the element is defined. As a
result, for transitions & retained mode backends to work correctly, IDs
should be specified."

Use `CLAY_AUTO_ID` freely for **non-queried, non-floating, non-animated**
internal elements (the `CLAY_AUTO_ID` button body in
`ButtonComponent`). Use `CLAY_ID("Name")` for anything you'll
`Clay_PointerOver(...)` or `floating.parentId =` against. Use
`CLAY_IDI("Item", i)` for loops where you'll later target a specific
index. Use `CLAY_ID_LOCAL` / `CLAY_IDI_LOCAL` inside reusable components
to avoid collisions when the component is called many times.

Sources:
[README, Element IDs](https://github.com/nicbarker/clay/blob/main/README.md),
[DeepWiki Clay summary](https://deepwiki.com/nicbarker/clay).

### 2. Duplicate global IDs silently misbehave

"Using duplicate IDs may cause some functionality to misbehave, such as
if you're trying to attach a floating container to a specific element
with a duplicated ID, it may not attach to the one you expect." This is
a name-clash bug class that the type system can't catch — discipline or
`CLAY_IDI` is the only defence.

### 3. Slow `MeasureText` callback

The README emphasises that the text-measurement callback is on the hot
path: "It is essential that this function is as fast as possible." Clay
caches internally but text-heavy UIs will still call it a lot. Don't do
file I/O, allocations, or full shaping inside it — pre-compute glyph
metrics and look them up. (
[README, Text Measurement](https://github.com/nicbarker/clay/blob/main/README.md).)

### 4. Allocating inside the layout function

The whole point of the arena model is no `malloc` during a frame. If a
component needs transient memory (e.g., a formatted string for a
label), use a *frame arena* — the website demo defines
`FrameAllocateString(...)` for exactly this and resets the arena each
frame. Don't `malloc` per element per frame.

### 5. Assuming Clay handles input routing or focus

Clay only knows about *pointer position*. There is no keyboard focus,
no Tab traversal, no clipboard, no IME, no text input field. If your UI
needs those, you implement them on top of Clay — typically by storing
focus state in your app and rendering the focused element with a
distinct border/colour. Several HN commenters were surprised by this;
it's not a flaw, just a scope decision worth knowing up front.
([HN discussion](https://news.ycombinator.com/item?id=42463123).)

### 6. Treating it like retained mode

The "interface is declared and computed each frame" model means you
can't (e.g.) build a UI tree once at startup and then mutate it. Every
frame you re-walk the entire tree. Components should be cheap; if
something is expensive to compute, cache it in your app state, not in
Clay.

### 7. Over-extracting micro-components

The canonical examples are notably *not* full of tiny `Padded()`,
`Centered()`, `Bordered()` wrappers. Inline designated initialisers are
already concise:

```c
.layout = { .padding = CLAY_PADDING_ALL(16),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER } }
```

Wrapping that in a `Centered(...)` helper adds indirection without
removing characters. Extract a function when the same *combination* of
config + children recurs, not for single-property tweaks.

### 8. Macro readability concerns

Several HN commenters noted that deeply nested `CLAY()` blocks with
inline configs are harder to read than HTML/JSX. The accepted answer
from the community: "If you don't use some kind of layouting language
(like XML/HTML), this is inevitably what you will _always_ end up with"
— same as Qt, Swing, AWT widget-construction code. The mitigation is
extracting components and named style structs *before* nesting gets
absurd, not "find a cleverer macro." ([HN](https://news.ycombinator.com/item?id=42463123).)

---

## References

Primary sources (highest authority):

- [nicbarker/clay README on GitHub](https://github.com/nicbarker/clay/blob/main/README.md)
  — the most authoritative source. Has the full API surface, the
  Quick Start, all macro definitions, the hover/floating/transition
  examples. Read this end to end.
- [examples/clay-official-website/main.c](https://github.com/nicbarker/clay/blob/main/examples/clay-official-website/main.c)
  — the single best "real" Clay codebase: the literal source of
  nicbarker.com/clay. Shows component decomposition (`LandingPageBlob`,
  feature blocks, renderer page), mobile/desktop branching, scroll, and
  animation in one file.
- [examples/raylib-sidebar-scrolling-container/main.c](https://github.com/nicbarker/clay/blob/main/examples/raylib-sidebar-scrolling-container/main.c)
  — canonical hover-aware button (`HeaderButtonStyle(bool hovered)` +
  `RenderHeaderButton`), manual scrollbar, dropdown via floating
  element. Best reference for interaction idioms.
- [examples/shared-layouts/clay-video-demo.c](https://github.com/nicbarker/clay/blob/main/examples/shared-layouts/clay-video-demo.c)
  — the layout used in the "Introducing Clay" YouTube video. Smaller,
  more digestible than the website demo. Defines `RenderHeaderButton`
  and `RenderDropdownMenuItem` as helpers.
- [examples/SDL3-simple-demo/main.c](https://github.com/nicbarker/clay/blob/main/examples/SDL3-simple-demo/main.c)
  — minimal SDL3 wiring. Shows the per-frame loop:
  `Clay_SetPointerState` → `Clay_UpdateScrollContainers` →
  `Clay_BeginLayout` → declarations → `Clay_EndLayout` → render.
- [examples/cpp-project-example/main.cpp](https://github.com/nicbarker/clay/blob/main/examples/cpp-project-example/main.cpp)
  — confirms there's no special C++ idiom; the C macro style is the
  idiom even from C++.

Secondary sources / commentary:

- [Clay on DeepWiki](https://deepwiki.com/nicbarker/clay) — useful for
  the architecture overview and the immediate-vs-retained contrast.
- [Simon Willison's blog post on Clay](https://simonwillison.net/2024/Dec/21/clay-ui-library/)
  — short, but captures the "declarative C is surprisingly readable"
  reaction and includes a real syntax-page code snippet.
- [HN discussion (Dec 2024)](https://news.ycombinator.com/item?id=42463123)
  — community pushback and counterpoints on macro readability, scope
  (no widgets, no input routing), and comparisons to other layout
  engines (Yoga, Taffy).
- [Earlier HN thread (Aug 2024)](https://news.ycombinator.com/item?id=41338946)
  — initial reception.
- [nicbarker.com/clay](https://www.nicbarker.com/clay) — the live demo
  page; the *output* of `clay-official-website/main.c` compiled to
  WebAssembly. Worth opening alongside the source to see what each
  function produces.

Talks (didn't get transcripts, but worth the parent agent watching if
relevant):

- ["Introducing Clay - High Performance UI Layout in C"](https://www.youtube.com/watch?v=DYWTw19_8r4)
  — Nic Barker's intro talk. Pair this with the video demo source.
- ["How Clay's UI Layout Algorithm Works"](https://www.youtube.com/watch?v=by9lQvpvMIc)
  — deep dive on the layout algorithm itself (relevant if performance
  tuning or trying to understand `GROW`/`FIT` propagation).
- ["Why Use C for UI Library?"](https://www.youtube.com/watch?v=8ZlN07IvoPI)
  — Nic Barker on the language-choice rationale.
- ["Programming UI"](https://www.youtube.com/watch?v=XYFBOIr6n_s) —
  panel with Nic Barker (Clay), Ray (raylib), Anton Mikhailov (Dreams).
  Good for design-philosophy context.

Note on the unrelated "Clay UI" by Liferay (`clayui.com`): that's a
totally different React component library from this Clay. Ignore any
search result from `clayui.com` or `clay.global` — they're not Nic
Barker's project.
