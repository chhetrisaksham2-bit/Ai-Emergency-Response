// Tiny test helper used by every *_test.cpp file in Phase 2.
// Owner: Utkarsh Kashyap (Organisation and interlinking)
// CHECK(condition, "what is being tested") prints PASS or FAIL.
// finish("part name") prints the totals and returns the exit code for main().
#pragma once

#include <iostream>
#include <string>

namespace phase2 {
namespace test {

inline int& passed() { static int n = 0; return n; }
inline int& failed() { static int n = 0; return n; }

inline void check(bool ok, const std::string& what) {
    if (ok) {
        ++passed();
        std::cout << "  PASS  " << what << "\n";
    } else {
        ++failed();
        std::cout << "  FAIL  " << what << "\n";
    }
}

inline int finish(const std::string& part) {
    std::cout << "\n[" << part << "] " << passed() << " passed, " << failed() << " failed\n";
    return failed() == 0 ? 0 : 1;
}

}  // namespace test
}  // namespace phase2

#define CHECK(condition, message) phase2::test::check((condition), (message))
