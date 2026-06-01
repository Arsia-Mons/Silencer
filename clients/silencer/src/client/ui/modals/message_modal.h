#ifndef MESSAGE_MODAL_H
#define MESSAGE_MODAL_H

#include "client/ui/retained/RetainedFrame.h"
#include "modal.h"
#include <functional>
#include <memory>
#include <string>

// Centred message overlay with a single OK button. When OK is clicked the
// onClose callback (if any) fires and the modal pops itself off the stack.
//
// In progress mode (no OK button) the caller updates the text each frame via
// SetText and pops the modal explicitly when whatever it's waiting on
// finishes. Used by the create-game flow's "Uploading map..." spinner.
class MessageModal : public Modal
{
public:
	MessageModal(std::string message, std::function<void()> onClose = nullptr);
	static std::unique_ptr<MessageModal> Progress(std::string message);

	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

	bool IsProgress() const { return !hasOk; }
	void SetText(const std::string & text);

private:
	MessageModal(std::string message, bool ok, std::function<void()> onClose);
	void Close();

	std::string message;
	bool hasOk;
	std::function<void()> onClose;
	silencer::client_ui::RetainedFrame retainedFrame_;
};

#endif
