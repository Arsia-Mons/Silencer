#ifndef PASSWORD_MODAL_H
#define PASSWORD_MODAL_H

#include "client/ui/retained/RetainedFrame.h"
#include "modal.h"
#include <functional>
#include <string>

// Modal that prompts the user for a password (single text input + OK button).
// On submit (OK click or Enter), the modal pops itself and forwards the
// captured text to the supplied callback.
class PasswordModal : public Modal
{
public:
	explicit PasswordModal(std::function<void(const char * password)> onSubmit);

	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, const silencer::ui::UiInputState& input, Uint8 hudPhase, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

private:
	void Submit();

	std::function<void(const char *)> onSubmit;
	char password[21] = {};
	std::string passwordDisplay_;
	silencer::client_ui::RetainedFrame retainedFrame_;
};

#endif
