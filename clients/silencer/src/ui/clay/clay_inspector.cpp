#include "clay_inspector.h"

#include <cctype>
#include <cstring>

namespace silencer::ui::clay_inspector {

namespace {

std::vector<Widget> & Registry() {
	static std::vector<Widget> v;
	return v;
}

bool IEq(const char * a, const char * b) {
	if(!a || !b) return false;
	while(*a && *b){
		if(std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
		++a; ++b;
	}
	return *a == 0 && *b == 0;
}

}  // namespace

void BeginFrame() { Registry().clear(); }

void Register(const Widget & w) { Registry().push_back(w); }

const std::vector<Widget> & All() { return Registry(); }

const Widget * FindByLabel(const char * label) {
	if(!label || !*label) return nullptr;
	const auto & v = Registry();
	const Widget * hit = nullptr;
	int count = 0;
	for(const auto & w : v){
		if(w.label && IEq(w.label, label)){
			hit = &w;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

}  // namespace silencer::ui::clay_inspector
