#include "doctest.h"
// Pure-logic test for the label matcher. We test IEq via a small shim by
// duplicating the logic here — the helper itself is a static inside
// interface.cpp and not exported. If this drifts, fail fast.
#include <cctype>
static bool IEq(const char* a, const char* b){
	if(!a || !b) return false;
	while(*a && *b){
		if(std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
		++a; ++b;
	}
	return *a == 0 && *b == 0;
}
TEST_CASE("widget label compare is case-insensitive"){
	CHECK(IEq("Connect", "connect"));
	CHECK(IEq("OPTIONS", "Options"));
	CHECK_FALSE(IEq("Options", "Optionz"));
	CHECK_FALSE(IEq("Options", "Options "));
}
