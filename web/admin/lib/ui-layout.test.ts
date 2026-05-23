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
    const updated = insertChild(original, "MainMenuActionGroup", child);

    expect(findNode(original.root, "text-status")).toBeNull();
    expect(findNode(updated.root, "text-status")?.text).toBe("READY");
    expect(findNode(updated.root, "MainMenuActionGroup")?.children?.at(-1)?.id).toBe(
      "text-status",
    );
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
    expect(findNode(rejectedAfterRoot.root, "MainMenuActionGroup")?.id).toBe(
      "MainMenuActionGroup",
    );

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

  test("validates imported documents and exports a Clay scaffold", () => {
    const document = createDefaultUiDocument();
    const parsed = validateUiDocument(JSON.parse(JSON.stringify(document)));
    const snippet = exportClaySnippet(parsed);

    expect(snippet).toContain("BuildMainMenuUi");
    expect(snippet).toContain('CLAY_ID("MainMenuRoot")');
    expect(snippet).toContain('Button("Tutorial", "main_menu.tutorial")');
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
});
