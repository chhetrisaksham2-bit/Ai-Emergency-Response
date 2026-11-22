// Tests for the first-aid module. Build and run with: make test-firstaid
#include "../testkit/check.h"
#include "firstaid.h"

using namespace rahat;

int main() {
    testkit::section("FirstAidGuide");
    FirstAidGuide guide;

    CHECK(guide.addStep("BLEEDING", "ANY", "Apply firm direct pressure"));
    CHECK(guide.addStep("BLEEDING", "ANY", "Keep the patient lying down"));
    CHECK(guide.addStep("BLEEDING", "CRITICAL", "Apply a tourniquet above the wound"));
    CHECK(!guide.addStep("BLEEDING", "SEVERE", "unknown severity is rejected"));

    std::vector<std::string> minor = guide.stepsFor("BLEEDING", Severity::Minor);
    CHECK_EQ(minor.size(), 2u);  // only the general steps
    CHECK_EQ(minor[0], std::string("Apply firm direct pressure"));

    std::vector<std::string> critical = guide.stepsFor("BLEEDING", Severity::Critical);
    CHECK_EQ(critical.size(), 3u);  // general steps first, then critical
    CHECK_EQ(critical[2], std::string("Apply a tourniquet above the wound"));

    CHECK(guide.addStep("STROKE", "SERIOUS", "Note the time symptoms started"));
    CHECK_EQ(guide.stepsFor("STROKE", Severity::Serious).size(), 1u);
    CHECK(guide.stepsFor("STROKE", Severity::Critical).empty());
    CHECK(guide.stepsFor("DOG_BITE", Severity::Minor).empty());

    CHECK(guide.hasStepsFor("STROKE"));
    CHECK(!guide.hasStepsFor("DOG_BITE"));

    return testkit::summary("firstaid");
}
