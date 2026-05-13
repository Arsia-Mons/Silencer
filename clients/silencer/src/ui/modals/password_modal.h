#ifndef PASSWORD_MODAL_H
#define PASSWORD_MODAL_H

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
	void Draw(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleTextInput(ScreenContext & ctx, char ascii) override;
	bool HandleKeyPress(ScreenContext & ctx, char ascii) override;

	void NotifyOkClicked() { okClicked = true; }

private:
	std::function<void(const char *)> onSubmit;
	bool okClicked = false;
	char password[21] = {};
};

#endif
