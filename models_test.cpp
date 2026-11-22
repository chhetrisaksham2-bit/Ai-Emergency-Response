// Tests for the core models. Build and run with: make test-models
#include "../testkit/check.h"
#include "models.h"

using namespace rahat;

static void testClock() {
    testkit::section("Clock helpers");

    int m = -1;
    CHECK(parseClock("08:30", &m));
    CHECK_EQ(m, 510);
    CHECK(parseClock("0:05", &m));
    CHECK_EQ(m, 5);
    CHECK(parseClock("24:00", &m));
    CHECK_EQ(m, 1440);
    CHECK(!parseClock("24:30", &m));
    CHECK(!parseClock("8:5", &m));
    CHECK(!parseClock("ab:cd", &m));
    CHECK(!parseClock("12:60", &m));

    CHECK_EQ(formatClock(510), std::string("08:30"));
    CHECK_EQ(formatClock(1445.7), std::string("00:05"));  // next day wraps
    CHECK_EQ(minuteOfDay(-10), 1430);

    CHECK(inTimeWindow(600, 480, 1020));    // 10:00 in 08:00-17:00
    CHECK(!inTimeWindow(1020, 480, 1020));  // end is exclusive
    CHECK(inTimeWindow(1380, 1200, 480));   // 23:00 in overnight 20:00-08:00
    CHECK(inTimeWindow(300, 1200, 480));    // 05:00 in overnight shift
    CHECK(!inTimeWindow(600, 1200, 480));   // 10:00 not in overnight shift
    CHECK(inTimeWindow(123, 0, 1440));      // all day
}

static void testSimClock() {
    testkit::section("SimClock");
    SimClock clock(600, 60.0);  // starts 10:00, 1 real second = 1 simulated minute
    double start = clock.now();
    CHECK(start >= 600.0 && start < 600.5);
    CHECK_EQ(clock.speed(), 60.0);
}

static void testEntities() {
    testkit::section("Entities");

    Doctor night;
    night.shiftStart = 20 * 60;
    night.shiftEnd = 8 * 60;
    night.maxPatients = 3;
    night.activeLoad = 1;
    CHECK(night.onDuty(2 * 60));
    CHECK(!night.onDuty(12 * 60));
    CHECK_EQ(night.freeLoad(), 2);

    Hospital hospital;
    hospital.facilities = {"ICU", "CT_SCAN"};
    CHECK(hospital.hasFacility("ICU"));
    CHECK(!hospital.hasFacility("ANTIVENOM"));
    CHECK(hospital.hasAllFacilities({"CT_SCAN", "ICU"}));
    CHECK(!hospital.hasAllFacilities({"CT_SCAN", "CATH_LAB"}));
    CHECK(hospital.hasAllFacilities({}));

    Severity s;
    CHECK(parseSeverity("CRITICAL", &s) && s == Severity::Critical);
    CHECK(!parseSeverity("critical", &s));
    AmbulanceType t;
    CHECK(parseAmbulanceType("ALS", &t) && t == AmbulanceType::ALS);
    CHECK_EQ(toString(Severity::Serious), std::string("SERIOUS"));
    CHECK_EQ(humanize("GENERAL_SURGEON"), std::string("general surgeon"));
    CHECK_EQ(humanize("CT_SCAN"), std::string("CT scan"));
    CHECK_EQ(humanize("ICU"), std::string("ICU"));
    CHECK_EQ(humanize("X_RAY"), std::string("X-ray"));
    CHECK_EQ(toString(AmbulanceStatus::AtHospital), std::string("AT_HOSPITAL"));
}

int main() {
    testClock();
    testSimClock();
    testEntities();
    return testkit::summary("models");
}
