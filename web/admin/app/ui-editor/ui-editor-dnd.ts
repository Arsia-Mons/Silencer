import {
  canHaveChildren,
  type UiAxis,
  type UiMovePlacement,
  type UiNode,
} from '../../lib/ui-layout';

export const UI_NODE_DRAG_TYPE = 'application/silencer-ui-node-id';
export const UI_PALETTE_DRAG_TYPE = 'application/silencer-ui-kind';

export type DropAxis = 'horizontal' | 'vertical';

type DropRect = {
  left: number;
  top: number;
  width: number;
  height: number;
};

type DropEvent = {
  clientX: number;
  clientY: number;
  currentTarget: {
    getBoundingClientRect(): DropRect;
  };
};

export function dropAxisFromDirection(direction?: UiAxis): DropAxis {
  return direction === 'row' ? 'horizontal' : 'vertical';
}

export function resolveMovePlacement(
  node: UiNode,
  point: { x: number; y: number },
  bounds: { width: number; height: number },
  axis: DropAxis,
): UiMovePlacement {
  if (node.kind === 'screen') return 'inside';

  const size = axis === 'horizontal' ? bounds.width : bounds.height;
  const offset = axis === 'horizontal' ? point.x : point.y;
  if (size > 0) {
    const ratio = offset / size;
    if (ratio < 0.25) return 'before';
    if (ratio > 0.75) return 'after';
  }

  return canHaveChildren(node.kind) ? 'inside' : 'after';
}

export function resolveEventMovePlacement(event: DropEvent, node: UiNode, axis: DropAxis): UiMovePlacement {
  const rect = event.currentTarget.getBoundingClientRect();
  return resolveMovePlacement(node, {
    x: event.clientX - rect.left,
    y: event.clientY - rect.top,
  }, {
    width: rect.width,
    height: rect.height,
  }, axis);
}
