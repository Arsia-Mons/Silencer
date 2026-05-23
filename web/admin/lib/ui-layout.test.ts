import { describe, expect, test } from "bun:test";
import {
  createDefaultUiDocument,
  createNode,
  duplicateNode,
  exportClaySnippet,
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
    const updated = insertChild(original, "main-menu-panel", child);

    expect(findNode(original.root, "text-status")).toBeNull();
    expect(findNode(updated.root, "text-status")?.text).toBe("READY");
    expect(findNode(updated.root, "main-menu-panel")?.children?.at(-1)?.id).toBe("text-status");
  });

  test("does not remove the root node", () => {
    const document = createDefaultUiDocument();
    const updated = removeNode(document, document.root.id);
    expect(updated.root.id).toBe(document.root.id);
    expect(updated.root.children?.length).toBe(document.root.children?.length);
  });

  test("duplicates a selected subtree with new ids", () => {
    const document = createDefaultUiDocument();
    const updated = duplicateNode(document, "main-menu-panel");
    const rootChildren = updated.root.children ?? [];

    expect(rootChildren).toHaveLength(3);
    expect(rootChildren[2].id).not.toBe("main-menu-panel");
    expect(rootChildren[2].children?.[0].id).not.toBe("button-host-game");
  });

  test("moves existing nodes without allowing invalid cycles", () => {
    const document = createDefaultUiDocument();
    const text = createNode("text", "status", { text: "READY" });
    const withText = insertChild(document, "main-menu-root", text);
    const movedIntoPanel = moveNode(withText, "text-status", {
      targetId: "main-menu-panel",
      placement: "inside",
    });

    expect(findNode(movedIntoPanel.root, "main-menu-panel")?.children?.at(-1)?.id).toBe(
      "text-status",
    );
    expect(
      findNode(movedIntoPanel.root, "main-menu-root")?.children?.some(
        (child) => child.id === "text-status",
      ),
    ).toBe(false);

    const reorderedButtons = moveNode(movedIntoPanel, "button-options", {
      targetId: "button-host-game",
      placement: "before",
    });
    expect(
      findNode(reorderedButtons.root, "main-menu-panel")
        ?.children?.map((child) => child.id)
        .slice(0, 3)
        .join(","),
    ).toBe("button-options,button-host-game,button-join-game");

    const panelA = createNode("panel", "alpha");
    const panelB = createNode("panel", "beta");
    const withPanels = insertChild(
      insertChild(document, "main-menu-root", panelA),
      "main-menu-root",
      panelB,
    );
    const reorderedContainers = moveNode(withPanels, "panel-alpha", {
      targetId: "panel-beta",
      placement: "after",
    });
    expect(
      findNode(reorderedContainers.root, "main-menu-root")
        ?.children?.map((child) => child.id)
        .slice(-2)
        .join(","),
    ).toBe("panel-beta,panel-alpha");

    const rejectedRootMove = moveNode(movedIntoPanel, "main-menu-root", {
      targetId: "main-menu-panel",
      placement: "inside",
    });
    expect(rejectedRootMove).toBe(movedIntoPanel);

    const rejectedBeforeRoot = moveNode(movedIntoPanel, "main-menu-panel", {
      targetId: "main-menu-root",
      placement: "before",
    });
    expect(rejectedBeforeRoot).toBe(movedIntoPanel);
    expect(findNode(rejectedBeforeRoot.root, "main-menu-panel")?.id).toBe("main-menu-panel");

    const rejectedAfterRoot = moveNode(movedIntoPanel, "main-menu-panel", {
      targetId: "main-menu-root",
      placement: "after",
    });
    expect(rejectedAfterRoot).toBe(movedIntoPanel);
    expect(findNode(rejectedAfterRoot.root, "main-menu-panel")?.id).toBe("main-menu-panel");

    const rejectedCycle = moveNode(movedIntoPanel, "main-menu-panel", {
      targetId: "text-status",
      placement: "inside",
    });
    expect(rejectedCycle).toBe(movedIntoPanel);

    const rejectedLeafInside = moveNode(movedIntoPanel, "button-options", {
      targetId: "text-status",
      placement: "inside",
    });
    expect(rejectedLeafInside).toBe(movedIntoPanel);
  });

  test("validates imported documents and exports a Clay scaffold", () => {
    const document = createDefaultUiDocument();
    const parsed = validateUiDocument(JSON.parse(JSON.stringify(document)));
    const snippet = exportClaySnippet(parsed);

    expect(snippet).toContain("BuildMainMenuUi");
    expect(snippet).toContain('CLAY_ID("main-menu-root")');
    expect(snippet).toContain('Button("HOST GAME", "open-host-game")');
  });

  test("rejects duplicate imported node ids", () => {
    const document = createDefaultUiDocument();
    const imported = JSON.parse(JSON.stringify(document));
    imported.root.children[1].children[0].id = "main-menu-title";

    expect(() => validateUiDocument(imported)).toThrow("Duplicate node id: main-menu-title");
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

    expect(() => validateUiDocument(imported)).toThrow("Node main-menu-root has invalid justify.");
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
    document.root.children[0].text = {};
    expect(() => validateUiDocument(document)).toThrow("Node main-menu-title has invalid text.");

    const actionDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    actionDocument.root.children[1].children[0].action = 42;
    expect(() => validateUiDocument(actionDocument)).toThrow(
      "Node button-host-game has invalid action.",
    );
  });

  test("rejects style fields unsupported by the client primitive preview", () => {
    const document = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    document.root.children[1].children[0].style.font = "title";
    expect(() => validateUiDocument(document)).toThrow(
      "Node button-host-game has unsupported font style.",
    );

    const cssDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    cssDocument.root.style.background = "#00ff00";
    expect(() => validateUiDocument(cssDocument)).toThrow(
      "Node main-menu-root has unsupported background style.",
    );

    const heightDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    heightDocument.root.children[1].children[0].style.height = { mode: "fixed", value: 48 };
    expect(() => validateUiDocument(heightDocument)).toThrow(
      "Node button-host-game button height must be fit.",
    );
  });
});
