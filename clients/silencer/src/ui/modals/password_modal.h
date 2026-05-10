#ifndef PASSWORD_MODAL_H
#define PASSWORD_MODAL_H

#include "modal.h"
#include <functional>

// Modal that prompts the user for a password (single text input + OK button).
// On submit (OK click or Enter), the modal pops itself and forwards the
// captured text to the supplied callback.
class PasswordModal : public Modal
{
public:
	explicit PasswordModal(std::function<void(const char * password)> onSubmit);

	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	std::function<void(const char *)> onSubmit;
	Uint16 inputId = 0;
	Uint16 okButtonId = 0;
};

#endif
