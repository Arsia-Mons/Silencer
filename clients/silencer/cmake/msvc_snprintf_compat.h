#pragma once
// MSVC 14.44 (VS 2022 17.13+) removed non-standard CRT functions (_snprintf,
// _vsnprintf, etc.) from the std:: namespace. nlohmann/json calls std::_snprintf
// internally in its lexer. Re-expose it so the build doesn't break regardless
// of nlohmann/json version. This header is force-included via /FI in CMakeLists.
#if defined(_MSC_VER)
// M_PI and friends require this on MSVC before any math header is included.
#define _USE_MATH_DEFINES
#include <stdio.h>
namespace std {
    using ::_snprintf;
}
#endif
