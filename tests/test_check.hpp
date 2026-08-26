#pragma once

#include <cstdlib>
#include <iostream>

namespace opentony::test {

[[noreturn]] inline void fail(
    const char* expression,
    const char* file,
    int line) {
    std::cerr << file << ':' << line << ": CHECK(" << expression
              << ") failed\n";
    std::abort();
}

} // namespace opentony::test

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            ::opentony::test::fail(#expression, __FILE__, __LINE__);          \
        }                                                                     \
    } while (false)
