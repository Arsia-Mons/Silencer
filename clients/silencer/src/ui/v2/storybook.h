#ifndef SILENCER_UI_V2_STORYBOOK_H
#define SILENCER_UI_V2_STORYBOOK_H

// The storybook is a self-contained SDL window — a playground where you
// can drop ui/v2 primitives onto a canvas, drag them around, resize them
// via a corner handle, edit their properties through a side panel, and
// see the v2 render path light them up live. Mirrors the shape of
// `Game::RunPreview()` (no PPM-dump mode — the storybook is interactive
// only) and shares the engine's Resources / Renderer / palette so the
// rendered primitives look identical to the in-game UI.
//
// Implementation lives in storybook.cpp; the entry point is declared on
// `Game` (see game.h) so main.cpp's existing preview dispatch can route
// `--preview-screen storybook` to it without knowing about ui::v2.

#endif
