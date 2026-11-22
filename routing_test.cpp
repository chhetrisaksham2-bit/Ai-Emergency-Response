// Tests for the routing module. Build and run with: make test-routing
#include <cmath>

#include "../testkit/check.h"
#include "routing.h"

using namespace rahat;

// Hospital A --(market road)-- Market Chowk --(market road)-- Patient Colony
//      \                                                        /
//       ---------- Ring Road Junction (longer, but no market) ---
static void buildMap(CityMap& map) {
    std::string error;
    map.addLandmark(1, "Hospital A", &error);
    map.addLandmark(2, "Market Chowk", &error);
    map.addLandmark(3, "Ring Road Junction", &error);
    map.addLandmark(4, "Patient Colony", &error);
    map.addLandmark(5, "Island (no roads)", &error);

    map.addRoad(1, 2, {"R1", 2.0, 30, TrafficLevel::Clear, "MARKET"}, &error);     // 4 min when clear
    map.addRoad(2, 4, {"R2", 1.0, 30, TrafficLevel::Clear, "MARKET"}, &error);     // 2 min when clear
    map.addRoad(1, 3, {"R3", 3.0, 60, TrafficLevel::Clear, ""}, &error);           // 3 min
    map.addRoad(3, 4, {"R4", 3.0, 60, TrafficLevel::Moderate, ""}, &error);        // 3 * 1.4 = 4.2 min

    map.traffic().addSlot("MARKET", 17 * 60, 21 * 60, TrafficLevel::Jam, &error);  // evening jam
}

static std::vector<std::string> names(const DoublyLinkedList<Checkpoint>& route) {
    std::vector<std::string> result;
    route.forEach([&](const Checkpoint& c) { result.push_back(c.name); });
    return result;
}

static void testTrafficPatterns() {
    testkit::section("TrafficPatterns");
    TrafficPatterns traffic;
    std::string error;

    CHECK(traffic.addSlot("SCHOOL", 8 * 60, 9 * 60, TrafficLevel::Heavy, &error));
    CHECK(traffic.addSlot("SCHOOL", 14 * 60, 15 * 60, TrafficLevel::Moderate, &error));
    CHECK(!traffic.addSlot("SCHOOL", 8 * 60 + 30, 10 * 60, TrafficLevel::Jam, &error));  // overlaps 08:00-09:00
    CHECK(traffic.addSlot("NIGHT", 22 * 60, 6 * 60, TrafficLevel::Clear, &error));       // crosses midnight
    CHECK(!traffic.addSlot("NIGHT", 5 * 60, 7 * 60, TrafficLevel::Heavy, &error));       // overlaps 00:00-06:00
    CHECK(!traffic.addSlot("BAD", 600, 600, TrafficLevel::Heavy, &error));

    CHECK(traffic.levelAt("SCHOOL", 8 * 60 + 15, TrafficLevel::Clear) == TrafficLevel::Heavy);
    CHECK(traffic.levelAt("SCHOOL", 9 * 60, TrafficLevel::Clear) == TrafficLevel::Clear);  // slot ended
    CHECK(traffic.levelAt("SCHOOL", 14 * 60 + 59, TrafficLevel::Clear) == TrafficLevel::Moderate);
    CHECK(traffic.levelAt("SCHOOL", 7 * 60, TrafficLevel::Moderate) == TrafficLevel::Moderate);  // before any slot
    CHECK(traffic.levelAt("NIGHT", 23 * 60, TrafficLevel::Heavy) == TrafficLevel::Clear);
    CHECK(traffic.levelAt("NIGHT", 3 * 60, TrafficLevel::Heavy) == TrafficLevel::Clear);
    CHECK(traffic.levelAt("NIGHT", 12 * 60, TrafficLevel::Heavy) == TrafficLevel::Heavy);
    CHECK(traffic.levelAt("", 12 * 60, TrafficLevel::Jam) == TrafficLevel::Jam);
    CHECK(traffic.levelAt("UNKNOWN", 12 * 60, TrafficLevel::Clear) == TrafficLevel::Clear);
    CHECK(traffic.hasPattern("NIGHT"));
}

static void testMapValidation() {
    testkit::section("CityMap validation");
    CityMap map;
    buildMap(map);
    std::string error;

    CHECK_EQ(map.landmarkCount(), 5);
    CHECK_EQ(map.roadCount(), 4);
    CHECK(!map.addLandmark(3, "Duplicate", &error));
    CHECK(!map.addRoad(1, 99, {"BAD1", 1.0, 30, TrafficLevel::Clear, ""}, &error));
    CHECK(!map.addRoad(2, 2, {"BAD2", 1.0, 30, TrafficLevel::Clear, ""}, &error));
    CHECK(!map.addRoad(1, 2, {"BAD3", 1.0, 0, TrafficLevel::Clear, ""}, &error));
    CHECK_EQ(map.landmarkName(3), std::string("Ring Road Junction"));
    CHECK_EQ(map.landmarkName(42), std::string("Unknown landmark"));
}

static void testFastestRouteChangesWithTraffic() {
    testkit::section("Dijkstra with traffic");
    CityMap map;
    buildMap(map);

    // 10:00 - market is clear, so the short market road is also the fastest.
    PathTree morning = map.shortestFrom(4, 10 * 60);
    CHECK_NEAR(map.minutesTo(morning, 1), 6.0, 1e-9);
    CHECK_NEAR(map.kmTo(morning, 1), 3.0, 1e-9);

    DoublyLinkedList<Checkpoint> toPatient = map.routeToSource(morning, 1, 100.0);
    std::vector<std::string> expectedOut = {"Hospital A", "Market Chowk", "Patient Colony"};
    CHECK(names(toPatient) == expectedOut);
    CHECK_NEAR(toPatient.front().atMinute, 100.0, 1e-9);
    CHECK_NEAR(toPatient.head()->next->value.atMinute, 104.0, 1e-9);
    CHECK_NEAR(toPatient.back().atMinute, 106.0, 1e-9);

    DoublyLinkedList<Checkpoint> toHospital = map.routeFromSource(morning, 1, 200.0);
    std::vector<std::string> expectedBack = {"Patient Colony", "Market Chowk", "Hospital A"};
    CHECK(names(toHospital) == expectedBack);
    CHECK_NEAR(toHospital.front().atMinute, 200.0, 1e-9);
    CHECK_NEAR(toHospital.head()->next->value.atMinute, 202.0, 1e-9);
    CHECK_NEAR(toHospital.back().atMinute, 206.0, 1e-9);

    // 18:00 - market road is jammed (x3), so the longer ring road is faster.
    PathTree evening = map.shortestFrom(4, 18 * 60);
    CHECK_NEAR(map.minutesTo(evening, 1), 7.2, 1e-9);
    CHECK_NEAR(map.kmTo(evening, 1), 6.0, 1e-9);
    std::vector<std::string> ringRoute = {"Hospital A", "Ring Road Junction", "Patient Colony"};
    CHECK(names(map.routeToSource(evening, 1, 0)) == ringRoute);

    // The shortest-distance route at 18:00 would take 18 minutes.
    PathTree byDistance = map.shortestFrom(4, 18 * 60, RouteMetric::Distance);
    CHECK_NEAR(map.kmTo(byDistance, 1), 3.0, 1e-9);
    CHECK_NEAR(map.minutesTo(byDistance, 1), 18.0, 1e-9);

    // Unreachable landmark
    CHECK(std::isinf(map.minutesTo(evening, 5)));
    CHECK(map.routeToSource(evening, 5, 0).empty());
    CHECK(map.routeFromSource(evening, 5, 0).empty());

    // Patient and hospital at the same landmark
    PathTree here = map.shortestFrom(1, 10 * 60);
    CHECK_EQ(map.routeToSource(here, 1, 50).size(), 1);
    CHECK_NEAR(map.minutesTo(here, 1), 0.0, 1e-9);
}

int main() {
    testTrafficPatterns();
    testMapValidation();
    testFastestRouteChangesWithTraffic();
    return testkit::summary("routing");
}
