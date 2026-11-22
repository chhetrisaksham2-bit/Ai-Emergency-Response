// Tests for the console helpers. Build and run with: make test-console
#include <sstream>

#include "../testkit/check.h"
#include "console.h"

using namespace rahat;

int main() {
    testkit::section("Colours");
    console::setColors(false);
    CHECK_EQ(console::red("CRITICAL"), std::string("CRITICAL"));
    console::setColors(true);
    CHECK(console::red("CRITICAL") != "CRITICAL");
    CHECK(console::bySeverity(Severity::Minor, "x").find("x") != std::string::npos);

    testkit::section("fit");
    CHECK_EQ(console::fit("A1", 5), std::string("A1   "));
    CHECK_EQ(console::fit("Valley Government Hospital", 10), std::string("Valley Go~"));
    CHECK_EQ(console::fit("exact", 5), std::string("exact"));

    testkit::section("readChoice");
    std::istringstream typed("hello\n9\n2x\n  \n3\n");
    std::ostringstream shown;
    CHECK_EQ(console::readChoice(typed, shown, "Pick", 1, 5), 3);
    CHECK(shown.str().find("Please type a number from 1 to 5") != std::string::npos);

    std::istringstream ended("");
    CHECK_EQ(console::readChoice(ended, shown, "Pick", 1, 5), -1);

    std::istringstream zero("0\n");
    CHECK_EQ(console::readChoice(zero, shown, "Menu", 0, 3), 0);

    return testkit::summary("console");
}
