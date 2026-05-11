#include "render_commands.h"

#include "renderer.h"
#include "surface.h"

#include <string>

namespace ui {

namespace {

inline int X(const Clay_BoundingBox & b) { return (int)b.x; }
inline int Y(const Clay_BoundingBox & b) { return (int)b.y; }
inline int W(const Clay_BoundingBox & b) { return (int)b.width; }
inline int H(const Clay_BoundingBox & b) { return (int)b.height; }

}  // namespace

void DrawRenderCommands(Clay_RenderCommandArray commands,
                        ::Renderer & renderer,
                        ::Surface & target,
                        int scale)
{
	for(int i = 0; i < commands.length; ++i){
		Clay_RenderCommand * cmd = Clay_RenderCommandArray_Get(&commands, i);
		const Clay_BoundingBox & bb = cmd->boundingBox;
		const DrawTag * tag = (const DrawTag *)cmd->userData;
		switch(cmd->commandType){
			case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
				const Clay_RectangleRenderData & r = cmd->renderData.rectangle;
				Uint8 color = (tag && tag->kind == DrawTagKind::Rect)
				              ? tag->rect.color : (Uint8)r.backgroundColor.r;
				renderer.DrawFilledRectangle(&target,
				    X(bb), Y(bb), X(bb)+W(bb), Y(bb)+H(bb), color, scale);
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_BORDER: {
				const Clay_BorderRenderData & b = cmd->renderData.border;
				Uint8 color = (Uint8)b.color.r;
				if(b.width.top    > 0){
					renderer.DrawFilledRectangle(&target,
					    X(bb), Y(bb), X(bb)+W(bb), Y(bb)+(int)b.width.top, color, scale);
				}
				if(b.width.bottom > 0){
					renderer.DrawFilledRectangle(&target,
					    X(bb), Y(bb)+H(bb)-(int)b.width.bottom, X(bb)+W(bb), Y(bb)+H(bb), color, scale);
				}
				if(b.width.left   > 0){
					renderer.DrawFilledRectangle(&target,
					    X(bb), Y(bb), X(bb)+(int)b.width.left, Y(bb)+H(bb), color, scale);
				}
				if(b.width.right  > 0){
					renderer.DrawFilledRectangle(&target,
					    X(bb)+W(bb)-(int)b.width.right, Y(bb), X(bb)+W(bb), Y(bb)+H(bb), color, scale);
				}
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_TEXT: {
				const Clay_TextRenderData & t = cmd->renderData.text;
				Uint8 bank  = (Uint8)t.fontId;
				Uint8 width = (Uint8)t.fontSize;
				Uint8 tint       = (tag && tag->kind == DrawTagKind::Text) ? tag->text.tint       : (Uint8)t.textColor.r;
				Uint8 brightness = (tag && tag->kind == DrawTagKind::Text) ? tag->text.brightness : 128;
				bool  alpha      = (tag && tag->kind == DrawTagKind::Text) ? tag->text.alpha      : false;
				bool  ramp       = (tag && tag->kind == DrawTagKind::Text) ? tag->text.ramp       : false;
				std::string buf(t.stringContents.chars, (size_t)t.stringContents.length);
				renderer.DrawText(&target, (Uint16)X(bb), (Uint16)Y(bb), buf.c_str(),
				                  bank, width, alpha, tint, brightness, ramp, scale);
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
				// Silencer sprites need bank+index, which Clay's image command
				// doesn't carry. Without a tag, this is a no-op.
				if(tag && tag->kind == DrawTagKind::Sprite){
					renderer.DrawSpriteAt(&target, tag->sprite.bank, tag->sprite.index,
					                      (Sint16)X(bb), (Sint16)Y(bb),
					                      tag->sprite.effect_color,
					                      tag->sprite.effect_brightness, scale);
				}
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
				int s = (scale > 1) ? scale : 1;
				target.PushScissor(X(bb)*s, Y(bb)*s, W(bb)*s, H(bb)*s);
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
				target.PopScissor();
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
				const Clay_CustomRenderData & c = cmd->renderData.custom;
				const DrawTag * ctag = (const DrawTag *)c.customData;
				if(!ctag) break;
				switch(ctag->kind){
					case DrawTagKind::Sprite:
						renderer.DrawSpriteAt(&target, ctag->sprite.bank, ctag->sprite.index,
						                      (Sint16)X(bb), (Sint16)Y(bb),
						                      ctag->sprite.effect_color,
						                      ctag->sprite.effect_brightness, scale);
						break;
					case DrawTagKind::None:
					case DrawTagKind::Text:
					case DrawTagKind::Rect:
						break;
				}
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_NONE:
				break;
		}
	}
}

}  // namespace ui
