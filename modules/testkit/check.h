// Tiny test helper shared by every module's test program.
// Usage:
//   CHECK(x > 0);
//   CHECK_EQ(heap.size(), 3);
//   return testkit::summary("ds");
#pragma once

#include <iostream>
#include <string>

namespace testkit {

inline int& failures() { static int count = 0; return count; }
inline int& passes() { static int count = 0; return count; }

inline void section(const std::string& name) {
    std::cout << "\n== " << name << " ==\n";
}

inline int summary(const std::string& moduleName) {
    std::cout << "\n[" << moduleName << "] " << passes() << " passed, "
              << failures() << " failed\n";
    return failures() == 0 ? 0 : 1;
}

}  // namespace testkit

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) {                                                          \
            ++testkit::passes();                                             \
        } else {                                                             \
            ++testkit::failures();                                           \
            std::cout << "  FAILED: " << #cond << "  (" << __FILE__ << ":"   \
                      << __LINE__ << ")\n";                                  \
        }                                                                    \
    } while (0)

#define CHECK_EQ(actual, expected)                                           \
    do {                                                                     \
        auto checkActual = (actual);                                         \
        auto checkExpected = (expected);                                     \
        if (checkActual == checkExpected) {                                  \
            ++testkit::passes();                                             \
        } else {                                                             \
            ++testkit::failures();                                           \
            std::cout << "  FAILED: " << #actual << " == " << #expected      \
                      << "  got [" << checkActual << "] expected ["          \
                      << checkExpected << "]  (" << __FILE__ << ":"          \
                      << __LINE__ << ")\n";                                  \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                              \
    do {                                                                     \
        double checkActual = (actual);                                       \
        double checkExpected = (expected);                                   \
        double diff = checkActual - checkExpected;                           \
        if (diff < 0) diff = -diff;                                          \
        if (diff <= (tolerance)) {                                           \
            ++testkit::passes();                                             \
        } else {                                                             \
            ++testkit::failures();                                           \
            std::cout << "  FAILED: " << #actual << " ~= " << #expected      \
                      << "  got [" << checkActual << "] expected ["          \
                      << checkExpected << "]  (" << __FILE__ << ":"          \
                      << __LINE__ << ")\n";                                  \
        }                                                                    \
    } while (0)
