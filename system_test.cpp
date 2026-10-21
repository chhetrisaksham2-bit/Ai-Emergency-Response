// Tests for the SYSTEM part. Owner: Saksham Chhetri.
// Run from the phase2 folder: make test   (or: ./build/system_test)
#include "../common/check.h"
#include "../dataset/dataset.h"
#include "rahat_system.h"

using namespace phase2;

int main() {
    std::string error;

    std::cout << "== Severity on a hand-made injury ==\n";
    Injury test;
    test.name = "Test";
    test.questions = {{"Q1", {{"no", 0}, {"yes", 3}}}, {"Q2", {{"no", 0}, {"yes", 5}}}};
    test.rules = {{Severity::Minor, 0, {}, "BLS"},
                  {Severity::Serious, 3, {"ICU"}, "BLS"},
                  {Severity::Critical, 8, {"ICU", "CT_SCAN"}, "ALS"}};
    RahatSystem empty;
    Assessment a;
    CHECK(empty.assess(test, {0, 0}, &a, &error) && a.points == 0 && a.severity == Severity::Minor, "0 points -> MINOR");
    CHECK(empty.assess(test, {1, 0}, &a, &error) && a.severity == Severity::Serious, "3 points -> SERIOUS (on the limit)");
    CHECK(empty.assess(test, {1, 1}, &a, &error) && a.points == 8 && a.severity == Severity::Critical &&
              a.ambulance == "ALS" && a.facilities.size() == 2,
          "8 points -> CRITICAL, needs ICU + CT scan, ALS ambulance");
    CHECK(!empty.assess(test, {0}, &a, &error), "too few answers is refused");
    CHECK(!empty.assess(test, {0, 5}, &a, &error), "an answer that does not exist is refused");

    std::cout << "\n== Whole system on the real data ==\n";
    Dataset data;
    RahatSystem rahat;
    bool ready = loadDataset("../phase2_data", &data, &error) && rahat.build(data, &error);
    CHECK(ready, "dataset loaded and city built " + error);
    if (!ready) return test::finish("system");

    // Patient at Clock Tower (1). Valley Government Hospital (H2) stands there.
    std::vector<HospitalOption> any = rahat.rankHospitals(1, {});
    CHECK(any.size() == 5, "all 5 hospitals are reachable from Clock Tower");
    CHECK(any.front().hospital->id == "H2" && any.front().km == 0, "nearest is H2, 0 km away");
    bool sorted = true;
    for (size_t i = 1; i < any.size(); ++i) sorted = sorted && any[i - 1].km <= any[i].km;
    CHECK(sorted, "hospitals are listed nearest first");

    // Critical stroke: face, arm, speech yes, conscious, < 4.5 h -> 8 points -> needs CT_SCAN + ICU.
    const Injury* stroke = nullptr;
    for (const Injury& injury : data.injuries)
        if (injury.code == "STROKE") stroke = &injury;
    CHECK(stroke != nullptr && rahat.assess(*stroke, {1, 1, 1, 0, 0}, &a, &error) && a.points == 8 &&
              a.severity == Severity::Critical,
          "stroke with every warning sign = 8 points -> CRITICAL");

    std::vector<HospitalOption> ranked = rahat.rankHospitals(1, a.facilities);
    const HospitalOption* chosen = RahatSystem::best(ranked);
    CHECK(chosen != nullptr && chosen->hospital->id == "H1" && chosen->km == 1.5,
          "goes to H1 City Care, 1.5 km (nearest with CT scan + ICU)");
    CHECK(chosen != nullptr && chosen->route == std::vector<int>({1, 6}), "route is Clock Tower -> Dilaram Chowk");
    CHECK(!ranked.front().canTreat, "H2 is nearer but cannot treat (no CT scan)");

    CHECK(RahatSystem::best(rahat.rankHospitals(1, {"SPACE_LAB"})) == nullptr, "no hospital when nobody has the facility");
    CHECK(rahat.rankHospitals(999, {}).empty(), "unknown landmark gives an empty list");

    return test::finish("system");
}
