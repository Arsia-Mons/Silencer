import { describe, expect, test } from "bun:test";
import {
  createDefaultUiDocument,
  createNode,
  duplicateNode,
  findNode,
  insertChild,
  moveNode,
  removeNode,
  validateUiDocument,
} from "./ui-layout";

describe("ui-layout model", () => {
  test("inserts children into container nodes immutably", () => {
    const original = createDefaultUiDocument();
    const child = createNode("text", "status", { text: "READY" });
    const updated = insertChild(original, "MainMenuActionGroup", child);

    expect(findNode(original.root, "text-status")).toBeNull();
    expect(findNode(updated.root, "text-status")?.text).toBe("READY");
    expect(findNode(updated.root, "MainMenuActionGroup")?.children?.at(-1)?.id).toBe("text-status");
  });

  test("does not remove the root node", () => {
    const document = createDefaultUiDocument();
    const updated = removeNode(document, document.root.id);
    expect(updated.root.id).toBe(document.root.id);
    expect(updated.root.children?.length).toBe(document.root.children?.length);
  });

  test("duplicates a selected subtree with new ids", () => {
    const document = createDefaultUiDocument();
    const updated = duplicateNode(document, "MainMenuActionGroup");
    const rootChildren = updated.root.children ?? [];

    expect(rootChildren).toHaveLength(4);
    expect(rootChildren[2].id).not.toBe("MainMenuActionGroup");
    expect(rootChildren[2].children?.[0].id).not.toBe("MainMenuActionStack");
  });

  test("moves existing nodes without allowing invalid cycles", () => {
    const document = createDefaultUiDocument();
    const text = createNode("text", "status", { text: "READY" });
    const withText = insertChild(document, "MainMenuRoot", text);
    const movedIntoPanel = moveNode(withText, "text-status", {
      targetId: "MainMenuActionGroup",
      placement: "inside",
    });

    expect(findNode(movedIntoPanel.root, "MainMenuActionGroup")?.children?.at(-1)?.id).toBe(
      "text-status",
    );
    expect(
      findNode(movedIntoPanel.root, "MainMenuRoot")?.children?.some(
        (child) => child.id === "text-status",
      ),
    ).toBe(false);

    const reorderedRows = moveNode(movedIntoPanel, "MainMenuOptionsRow", {
      targetId: "MainMenuTutorialRow",
      placement: "before",
    });
    expect(
      findNode(reorderedRows.root, "MainMenuActionStack")
        ?.children?.map((child) => child.id)
        .slice(0, 3)
        .join(","),
    ).toBe("MainMenuOptionsRow,MainMenuTutorialRow,MainMenuConnectToLobbyRow");

    const panelA = createNode("panel", "alpha");
    const panelB = createNode("panel", "beta");
    const withPanels = insertChild(
      insertChild(document, "MainMenuRoot", panelA),
      "MainMenuRoot",
      panelB,
    );
    const reorderedContainers = moveNode(withPanels, "panel-alpha", {
      targetId: "panel-beta",
      placement: "after",
    });
    expect(
      findNode(reorderedContainers.root, "MainMenuRoot")
        ?.children?.map((child) => child.id)
        .slice(-2)
        .join(","),
    ).toBe("panel-beta,panel-alpha");

    const rejectedRootMove = moveNode(movedIntoPanel, "MainMenuRoot", {
      targetId: "MainMenuActionGroup",
      placement: "inside",
    });
    expect(rejectedRootMove).toBe(movedIntoPanel);

    const rejectedBeforeRoot = moveNode(movedIntoPanel, "MainMenuActionGroup", {
      targetId: "MainMenuRoot",
      placement: "before",
    });
    expect(rejectedBeforeRoot).toBe(movedIntoPanel);
    expect(findNode(rejectedBeforeRoot.root, "MainMenuActionGroup")?.id).toBe(
      "MainMenuActionGroup",
    );

    const rejectedAfterRoot = moveNode(movedIntoPanel, "MainMenuActionGroup", {
      targetId: "MainMenuRoot",
      placement: "after",
    });
    expect(rejectedAfterRoot).toBe(movedIntoPanel);
    expect(findNode(rejectedAfterRoot.root, "MainMenuActionGroup")?.id).toBe("MainMenuActionGroup");

    const rejectedCycle = moveNode(movedIntoPanel, "MainMenuActionGroup", {
      targetId: "text-status",
      placement: "inside",
    });
    expect(rejectedCycle).toBe(movedIntoPanel);

    const rejectedLeafInside = moveNode(movedIntoPanel, "MainMenuOptionsButton", {
      targetId: "text-status",
      placement: "inside",
    });
    expect(rejectedLeafInside).toBe(movedIntoPanel);
  });

  test("validates imported documents with rebuildable screen primitives", () => {
    const document = createDefaultUiDocument();
    const parsed = validateUiDocument(JSON.parse(JSON.stringify(document)));

    expect(parsed.root.image?.bank).toBe(6);
    expect(parsed.root.image?.index).toBe(0);
    expect(parsed.root.image?.mode).toBe("normal");
    expect(findNode(parsed.root, "MainMenuSilencerLogo")?.component).toBe("main-menu.logo");
    expect(findNode(parsed.root, "MainMenuActionGroup")?.floating?.parentAttach).toBe("center");
    expect(findNode(parsed.root, "MainMenuVersion")?.textBinding).toBe("client.version");
  });

  test("rejects duplicate imported node ids", () => {
    const document = createDefaultUiDocument();
    const imported = JSON.parse(JSON.stringify(document));
    imported.root.children[0].id = "MainMenuRoot";

    expect(() => validateUiDocument(imported)).toThrow("Duplicate node id: MainMenuRoot");
  });

  test("rejects node kind names inherited from Object prototype", () => {
    const document = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    document.root.children[0].kind = "toString";

    expect(() => validateUiDocument(document)).toThrow("Unsupported node kind: toString");
  });

  test("rejects layout values that the client preview cannot render", () => {
    const document = createDefaultUiDocument();
    const imported = JSON.parse(JSON.stringify(document));
    imported.root.style.justify = "between";

    expect(() => validateUiDocument(imported)).toThrow("Node MainMenuRoot has invalid justify.");
  });

  test("rejects imported documents with missing or invalid viewport", () => {
    const missing = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    delete missing.viewport;
    expect(() => validateUiDocument(missing)).toThrow("Document viewport is missing.");

    const invalid = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    invalid.viewport.width = Number.NaN;
    expect(() => validateUiDocument(invalid)).toThrow("Document viewport width is invalid.");
  });

  test("rejects imported nodes with invalid optional scalar fields", () => {
    const document = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    document.root.children[2].text = {};
    expect(() => validateUiDocument(document)).toThrow("Node MainMenuVersion has invalid text.");

    const actionDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(actionDocument.root, "MainMenuTutorialButton") as any).action = 42;
    expect(() => validateUiDocument(actionDocument)).toThrow(
      "Node MainMenuTutorialButton has invalid action.",
    );
  });

  test("rejects style fields unsupported by the client primitive preview", () => {
    const document = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(document.root, "MainMenuTutorialButton") as any).style.font = "title";
    expect(() => validateUiDocument(document)).toThrow(
      "Node MainMenuTutorialButton has unsupported font style.",
    );

    const cssDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    cssDocument.root.style.background = "#00ff00";
    expect(() => validateUiDocument(cssDocument)).toThrow(
      "Node MainMenuRoot has unsupported background style.",
    );

    const heightDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(heightDocument.root, "MainMenuTutorialButton") as any).style.height = {
      mode: "fixed",
      value: 48,
    };
    expect(() => validateUiDocument(heightDocument)).toThrow(
      "Node MainMenuTutorialButton button height must be fit.",
    );
  });

  test("rejects invalid rebuild primitive metadata", () => {
    const floatingDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    delete (findNode(floatingDocument.root, "MainMenuActionGroup") as any).floating.parentAttach;
    expect(() => validateUiDocument(floatingDocument)).toThrow(
      "Node MainMenuActionGroup has invalid floating parentAttach.",
    );

    const pointerDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(pointerDocument.root, "MainMenuActionGroup") as any).floating.pointerPassthrough =
      "yes";
    expect(() => validateUiDocument(pointerDocument)).toThrow(
      "Node MainMenuActionGroup has invalid floating pointerPassthrough.",
    );

    const sizeDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    sizeDocument.root.style.width.min = 500;
    sizeDocument.root.style.width.max = 200;
    expect(() => validateUiDocument(sizeDocument)).toThrow(
      "Node MainMenuRoot width min cannot exceed max.",
    );
  });

  test("rejects fields that no client parser or renderer honors", () => {
    const documentField = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    documentField.debug = true;
    expect(() => validateUiDocument(documentField)).toThrow(
      "Document has unsupported field: debug.",
    );

    const viewportField = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    viewportField.viewport.aspectRatio = "4:3";
    expect(() => validateUiDocument(viewportField)).toThrow(
      "Document viewport has unsupported field: aspectRatio.",
    );

    const nodeField = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    nodeField.root.unsupportedLayoutMode = "grid";
    expect(() => validateUiDocument(nodeField)).toThrow(
      "Node MainMenuRoot has unsupported field: unsupportedLayoutMode.",
    );

    const imageField = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    imageField.root.image.tintPalette = 44;
    expect(() => validateUiDocument(imageField)).toThrow(
      "Node MainMenuRoot image has unsupported field: tintPalette.",
    );

    const floatingField = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(floatingField.root, "MainMenuActionGroup") as any).floating.anchor = "screen";
    expect(() => validateUiDocument(floatingField)).toThrow(
      "Node MainMenuActionGroup floating has unsupported field: anchor.",
    );

    const sizeField = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    sizeField.root.style.width.preferred = 640;
    expect(() => validateUiDocument(sizeField)).toThrow(
      "Node MainMenuRoot width sizing has unsupported field: preferred.",
    );
  });

  test("rejects surface tokens outside the manifest", () => {
    const unknownAction = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(unknownAction.root, "MainMenuTutorialButton") as any).action = "main_menu.credits";
    expect(() => validateUiDocument(unknownAction)).toThrow(
      "Node MainMenuTutorialButton references unknown action main_menu.credits.",
    );

    const unknownComponent = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(unknownComponent.root, "MainMenuSilencerLogo") as any).component = "main-menu.badge";
    expect(() => validateUiDocument(unknownComponent)).toThrow(
      "Node MainMenuSilencerLogo references unknown component main-menu.badge.",
    );

    const unknownBinding = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(unknownBinding.root, "MainMenuVersion") as any).textBinding = "client.badge";
    expect(() => validateUiDocument(unknownBinding)).toThrow(
      "Node MainMenuVersion references unknown text binding client.badge.",
    );

    const unknownSurface = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    unknownSurface.surface = "custom-screen";
    expect(() =>
      validateUiDocument(unknownSurface, { requireTokenManifestForSurfaceTokens: true }),
    ).toThrow("Surface custom-screen needs a UI token manifest.");
  });

  test("rejects fields that the client renderer does not honor on primitives", () => {
    const buttonImage = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(buttonImage.root, "MainMenuTutorialButton") as any).image = {
      bank: 6,
      index: 0,
    };
    expect(() => validateUiDocument(buttonImage)).toThrow(
      "Node MainMenuTutorialButton button cannot use image.",
    );

    const buttonSize = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(buttonSize.root, "MainMenuTutorialButton") as any).style.width.min = 160;
    expect(() => validateUiDocument(buttonSize)).toThrow(
      "Node MainMenuTutorialButton fixed width sizing cannot use min or max.",
    );

    const inputDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    inputDocument.root.children.push({
      id: "UnsupportedInput",
      kind: "input",
      name: "Unsupported Input",
      style: {
        width: { mode: "fixed", value: 180 },
        height: { mode: "fixed", value: 24 },
      },
    });
    expect(() => validateUiDocument(inputDocument)).toThrow("Unsupported node kind: input");

    const wrongKindField = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    wrongKindField.root.component = "main-menu.logo";
    expect(() => validateUiDocument(wrongKindField)).toThrow(
      "Node MainMenuRoot screen cannot use component.",
    );

    const fixedBounds = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(fixedBounds.root, "MainMenuTutorialSpacer") as any).style.width.max = 48;
    expect(() => validateUiDocument(fixedBounds)).toThrow(
      "Node MainMenuTutorialSpacer fixed width sizing cannot use min or max.",
    );

    const fitValue = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    (findNode(fitValue.root, "MainMenuActionGroup") as any).style.width.value = 220;
    expect(() => validateUiDocument(fitValue)).toThrow(
      "Node MainMenuActionGroup width value is only valid for fixed sizing.",
    );
  });
});
