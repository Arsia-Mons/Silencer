#pragma once

#include "client/ui/ClientUi.h"

namespace silencer {
namespace client_ui {
namespace internal {

struct DeferredUiMutationSink {
	ClientUi * client_ui = nullptr;

	bool submit(DeferredUiMutation mutation) const;
	const void * owner() const { return client_ui; }
	explicit operator bool() const { return client_ui != nullptr; }
};

DeferredUiMutationSink use_deferred_ui_mutations();

}  // namespace internal
}  // namespace client_ui
}  // namespace silencer
