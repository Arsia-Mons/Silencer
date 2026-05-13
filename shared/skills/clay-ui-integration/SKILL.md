---
name: clay-ui-integration
description: Use when designing, implementing, or auditing an application integration of nicbarker/clay, the renderer-agnostic C UI layout library. Applies to SDL, Raylib, Sokol, WebAssembly, custom engines, game UI, tools, or any code that must wire Clay lifecycle, input, scrolling, text measurement, IDs, render command dispatch, debug tooling, custom elements, or multiple contexts correctly.
---

# Clay UI Integration

## Core Model

Treat Clay as a frame-based layout and interaction system that produces render commands. It is not a windowing layer, event loop, renderer, widget toolkit, or application state manager.

Own these responsibilities outside Clay:

- Window/display size and coordinate normalization.
- Raw device input collection.
- Text input, keyboard/gamepad focus, accessibility, and automation metadata.
- Application state mutations and navigation.
- Rendering backend, texture/font resources, clipping, and draw ordering.

Use Clay for:

- Declaring the UI tree every frame.
- Computing responsive layout, text wrapping, clipping, scrolling offsets, hover state, and render commands.
- Emitting renderer-agnostic primitives: rectangle, border, text, image, custom, scissor.

## Required Lifecycle

Initialize once after you know the first layout dimensions and have a text measurement implementation:

```c
uint64_t size = Clay_MinMemorySize();
Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(size, memory);
Clay_Context *ctx = Clay_Initialize(arena, dimensions, errorHandler);
Clay_SetMeasureTextFunction(MeasureText, userData);
```

Run this sequence once per UI frame, before rendering:

```c
Clay_SetCurrentContext(ctx);                 // if more than one context exists
Clay_SetLayoutDimensions(dimensions);        // update on resize, safe every frame
Clay_SetPointerState(pointerPosition, down); // continuous current state
Clay_UpdateScrollContainers(enableDrag, scrollDelta, deltaTime);

Clay_BeginLayout();
// Declare the complete UI tree here.
Clay_RenderCommandArray commands = Clay_EndLayout();
RenderClayCommands(commands);
```

Do not reset pointer state to "not down" just to make frames deterministic. Clay needs continuity to distinguish pressed-this-frame, held, and released states.

## Input Integration

Normalize input once at the platform boundary, then feed Clay the UI-space pointer state every frame. Use the same coordinate space for `Clay_SetLayoutDimensions`, pointer position, hit testing, and rendering.

Pass scroll wheel/trackpad deltas to `Clay_UpdateScrollContainers` before `Clay_BeginLayout`; otherwise scroll containers lag or do not move. If the app owns scrolling itself, provide offsets through `.clip.childOffset` and treat Clay's scroll API as optional.

Use `Clay_Hovered()` only during declaration of the currently open element. Use `Clay_OnHover()` for pointer callbacks:

```c
void OnButton(Clay_ElementId id, Clay_PointerData data, void *user) {
    if (data.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        QueueUiAction(user);
    }
}

CLAY(CLAY_ID("SaveButton"), buttonStyle) {
    Clay_OnHover(OnButton, user);
    CLAY_TEXT(CLAY_STRING("Save"), textConfig);
}
```

For app state changes, queue typed intents and drain them after layout. Avoid destroying screens, reallocating layout-owned memory, or mutating domain state directly from inside the layout pass.

Clay does not provide full keyboard/gamepad focus or text editing policy. Build a small UI input layer that maps text input, focus traversal, confirm/cancel, and automation actions onto stable element IDs or your own component registry.

## IDs And Stability

Give interactive, animated, scrollable, floating, retained-rendered, or debugged elements stable unique IDs.

Use:

- `CLAY_ID("Name")` for static singleton elements.
- `CLAY_IDI("Row", stableIndexOrId)` for repeated elements.
- Local ID macros when composing reusable components that may appear multiple times under different parents.

Avoid duplicate IDs. Avoid using `CLAY_AUTO_ID()` for elements that need transitions, external queries, stable automation, retained rendering, or callback routing.

## Text Measurement

Clay is renderer-agnostic and must be given a measurement function. The function must match the renderer's actual text drawing behavior closely enough for wrapping and sizing to be correct.

Rules:

- `Clay_StringSlice` is not guaranteed to be null-terminated.
- Respect `fontId`, `fontSize`, and any supported spacing/wrapping options your renderer uses.
- Cache in your renderer layer if measurement is expensive.
- Call `Clay_ResetMeasureTextCache()` only when font metrics or relevant measurement behavior changes.

## Rendering Backend

Render by iterating `Clay_RenderCommandArray` and dispatching on `commandType`.

Your backend must handle:

- `RECTANGLE`: fills, colors, corner radii if supported.
- `BORDER`: per-side widths and corner behavior.
- `TEXT`: font lookup, text color, clipping, non-null-terminated slices.
- `IMAGE`: application-owned texture/image pointer and sizing policy.
- `CUSTOM`: tagged application payloads for primitives Clay does not know about.
- `SCISSOR_START` / `SCISSOR_END`: clip stack or equivalent renderer clipping.

Keep Clay-specific renderer code behind one boundary. UI screens/components should not call SDL, Raylib, GPU APIs, or renderer internals directly unless they are renderer adapter code.

## Custom Elements

Use `.custom.customData` for application-specific render primitives such as video, 3D previews, rich sprites, platform-native controls, or custom chrome. Store payloads in memory that remains valid until after render command consumption.

Use frame arenas for transient payloads:

```c
MyCustomPayload *p = FrameAlloc(sizeof(*p));
*p = payload;
CLAY(CLAY_ID("Preview"), { .custom = { .customData = p } }) {}
```

Tag payload types so the renderer can dispatch safely.

## Scrolling

For Clay-managed scrolling:

1. Call `Clay_SetPointerState`.
2. Call `Clay_UpdateScrollContainers(enableDrag, scrollDelta, deltaTime)`.
3. Configure the container with `.clip`.
4. Use `Clay_GetScrollOffset()` as `.clip.childOffset`.

For custom scrollbars or external scroll state, query `Clay_GetScrollContainerData()` after layout or manage `.childOffset` yourself.

## Multiple Contexts

Use one context unless there is a concrete reason to isolate independent UI instances. If using multiple contexts, store each `Clay_Context*`, call `Clay_SetCurrentContext()` before all Clay API calls for that instance, and never render/use contexts concurrently across threads.

## Verification Checklist

Before calling a Clay integration correct, verify:

- Pointer coordinates, layout dimensions, hit testing, and render output use one coordinate space.
- Pressed/held/released callbacks behave correctly across multiple frames.
- Wheel/trackpad scrolling and drag scrolling work without a one-frame delay.
- Text measurement matches actual rendering.
- Window resize updates layout dimensions before layout.
- Scissor commands clip all affected command types.
- IDs are unique and stable in loops, transitions, floating elements, and automation.
- Custom payload memory lives until render is complete.
- Application state mutations happen outside the layout declaration pass.
- Debug mode renders correctly through the same backend.

## References

Read [references/sources.md](references/sources.md) when you need source links, example files, or deeper implementation notes.
