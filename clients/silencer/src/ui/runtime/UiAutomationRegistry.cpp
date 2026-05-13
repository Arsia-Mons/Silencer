#include "runtime/UiAutomationRegistry.h"

#include <algorithm>
#include <cctype>

namespace silencer {
namespace ui {
namespace {

bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
	if(a.size() != b.size()) return false;
	for(size_t i = 0; i < a.size(); ++i) {
		char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
		char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
		if(ca != cb) return false;
	}
	return true;
}

}  // namespace

void UiAutomationRegistry::BeginFrame() {
	elements_.clear();
}

void UiAutomationRegistry::Register(UiElementMetadata metadata) {
	elements_.push_back(metadata);
}

const std::vector<UiElementMetadata>& UiAutomationRegistry::Elements() const {
	return elements_;
}

const UiElementMetadata* UiAutomationRegistry::FindById(const std::string& id) const {
	auto it = std::find_if(elements_.begin(), elements_.end(), [&](const UiElementMetadata& element) {
		return element.id == id;
	});
	return it == elements_.end() ? nullptr : &*it;
}

const UiElementMetadata* UiAutomationRegistry::FindByLabel(const std::string& label) const {
	auto it = std::find_if(elements_.begin(), elements_.end(), [&](const UiElementMetadata& element) {
		return EqualsIgnoreCase(element.label, label);
	});
	return it == elements_.end() ? nullptr : &*it;
}

}  // namespace ui
}  // namespace silencer
