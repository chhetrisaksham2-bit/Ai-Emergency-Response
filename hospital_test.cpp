// Tests for the hospital module: ranking, checkAll reasons, doctor load.
#include <limits>

#include "../testkit/check.h"
#include "hospital.h"

using namespace rahat;

static Doctor makeDoctor(const std::string& id, const std::string& hospital, const std::string& spec,
                         int start, int end, int maxPatients) {
    Doctor d;
    d.id = id;
    d.hospitalId = hospital;
    d.name = "Dr. " + id;
    d.specialization = spec;
    d.shiftStart = start;
    d.shiftEnd = end;
    d.maxPatients = maxPatients;
    return d;
}

// Three hospitals at landmarks 1, 2, 3.
//   H1 near (5 min): cardiology day shift only, ICU but no cath lab
//   H2 middle (9 min): cardiology all day, ICU + cath lab
//   H3 far (15 min): cardiology night shift, ICU + cath lab
static void buildHospitals(HospitalManager& manager) {
    std::string error;
    manager.addHospital("H1", "Near Clinic", 1, {"ICU"}, &error);
    manager.addHospital("H2", "Heart Centre", 2, {"ICU", "CATH_LAB"}, &error);
    manager.addHospital("H3", "Far Medical College", 3, {"ICU", "CATH_LAB"}, &error);

    manager.addDoctor(makeDoctor("D1", "H1", "CARDIOLOGIST", 8 * 60, 20 * 60, 2), &error);
    manager.addDoctor(makeDoctor("D2", "H2", "CARDIOLOGIST", 0, 1440, 1), &error);
    manager.addDoctor(makeDoctor("D3", "H2", "CARDIOLOGIST", 0, 1440, 2), &error);
    manager.addDoctor(makeDoctor("D4", "H3", "CARDIOLOGIST", 20 * 60, 8 * 60, 3), &error);
    manager.addDoctor(makeDoctor("D5", "H1", "EMERGENCY_PHYSICIAN", 0, 1440, 4), &error);
}

// Reserves one minor patient (1 load unit) at the given hospital's cardiologists.
static Doctor* reserve(HospitalManager& manager, Hospital& hospital, int minuteOfDay) {
    int units = 0;
    return manager.reserveDoctor(hospital, "CARDIOLOGIST", minuteOfDay, Severity::Minor, &units);
}

static double travel(int landmark) {
    switch (landmark) {
        case 1: return 5;
        case 2: return 9;
        case 3: return 15;
    }
    return std::numeric_limits<double>::infinity();
}

static Request cardiacRequest(const std::vector<std::string>& facilities) {
    Request r;
    r.id = "R1";
    r.injuryCode = "CARDIAC_ARREST";
    r.requiredSpecialization = "CARDIOLOGIST";
    r.requiredFacilities = facilities;
    r.requiredAmbulance = AmbulanceType::ALS;
    return r;
}

static std::vector<std::string> ids(const std::vector<HospitalCandidate>& candidates) {
    std::vector<std::string> result;
    for (const HospitalCandidate& c : candidates) result.push_back(c.hospital->id);
    return result;
}

static void testSetup() {
    testkit::section("Setup and validation");
    HospitalManager manager;
    buildHospitals(manager);
    std::string error;

    CHECK_EQ(manager.all().size(), 3u);
    CHECK(manager.addHospital("H1", "Duplicate", 9, {}, &error) == nullptr);
    CHECK(manager.addDoctor(makeDoctor("D9", "H404", "CARDIOLOGIST", 0, 1440, 1), &error) == nullptr);
    CHECK(manager.addDoctor(makeDoctor("D1", "H2", "CARDIOLOGIST", 0, 1440, 1), &error) == nullptr);
    CHECK(manager.addDoctor(makeDoctor("D8", "H2", "CARDIOLOGIST", 0, 1440, 0), &error) == nullptr);
    CHECK_EQ(manager.find("H2")->doctors.size(), 2u);

    Ambulance ambulance;
    ambulance.id = "A1";
    ambulance.hospitalId = "H3";
    CHECK(manager.linkAmbulance(&ambulance, &error));
    CHECK_EQ(manager.find("H3")->ambulances.size(), 1u);
    ambulance.hospitalId = "H404";
    CHECK(!manager.linkAmbulance(&ambulance, &error));
}

static void testRanking() {
    testkit::section("rankFor");
    HospitalManager manager;
    buildHospitals(manager);

    // Daytime, only ICU needed: all day-shift cardiologists count, nearest first.
    std::vector<std::string> day = ids(manager.rankFor(cardiacRequest({"ICU"}), 10 * 60, travel));
    std::vector<std::string> expectedDay = {"H1", "H2"};  // H3's cardiologist works nights
    CHECK(day == expectedDay);

    // Night, cath lab needed: H1 lacks the facility, H2 and H3 qualify.
    std::vector<HospitalCandidate> night = manager.rankFor(cardiacRequest({"ICU", "CATH_LAB"}), 23 * 60, travel);
    std::vector<std::string> expectedNight = {"H2", "H3"};
    CHECK(ids(night) == expectedNight);
    CHECK_NEAR(night[0].minutesToPatient, 9.0, 1e-9);

    // Nobody has a neurologist.
    Request stroke = cardiacRequest({});
    stroke.requiredSpecialization = "NEUROLOGIST";
    CHECK(manager.rankFor(stroke, 10 * 60, travel).empty());

    // Unreachable hospital is skipped.
    auto onlyH1Reachable = [](int landmark) {
        return landmark == 1 ? 5.0 : std::numeric_limits<double>::infinity();
    };
    std::vector<std::string> reachable = ids(manager.rankFor(cardiacRequest({"ICU"}), 10 * 60, onlyH1Reachable));
    CHECK_EQ(reachable.size(), 1u);
}

static void testCheckAllReasons() {
    testkit::section("checkAll: every hospital with a reason");
    HospitalManager manager;
    buildHospitals(manager);
    Request request = cardiacRequest({"ICU", "CATH_LAB"});
    int noon = 12 * 60;
    auto twoAmbulances = [](const std::string&) { return 2; };

    // Noon, cath lab needed: H1 has no cath lab, H3's cardiologist works nights.
    std::vector<HospitalCheck> checks = manager.checkAll(request, noon, travel, twoAmbulances);
    CHECK_EQ(checks.size(), 3u);
    CHECK_EQ(checks[0].hospital->id, std::string("H1"));  // nearest first, even when not suitable
    CHECK(!checks[0].suitable());
    CHECK_EQ(checks[0].reason, std::string("no cath lab"));
    CHECK(checks[1].suitable());
    CHECK_EQ(checks[1].capacity, 2);  // 3 cardiologist slots, but only 2 ambulances
    CHECK(checks[1].reason.empty());
    CHECK_EQ(checks[2].reason, std::string("no cardiologist on duty"));

    // No free ambulance at the suitable hospital.
    auto noAmbulances = [](const std::string& id) { return id == "H2" ? 0 : 2; };
    checks = manager.checkAll(request, noon, travel, noAmbulances);
    CHECK_EQ(checks[1].reason, std::string("no ALS ambulance available right now"));

    // Unreachable hospitals go last.
    auto onlyH3Reachable = [](int landmark) {
        return landmark == 3 ? 15.0 : std::numeric_limits<double>::infinity();
    };
    checks = manager.checkAll(cardiacRequest({"ICU"}), 23 * 60, onlyH3Reachable, twoAmbulances);
    CHECK_EQ(checks[0].hospital->id, std::string("H3"));
    CHECK(checks[0].suitable());
    CHECK_EQ(checks[1].reason, std::string("not reachable by road"));
    CHECK_EQ(checks[2].reason, std::string("not reachable by road"));
}

static void testCapacity() {
    testkit::section("reserve, release, busy specialist");
    HospitalManager manager;
    buildHospitals(manager);
    Hospital* heart = manager.find("H2");
    Request request = cardiacRequest({"ICU", "CATH_LAB"});
    int noon = 12 * 60;

    CHECK_EQ(manager.patientsFittable(*heart, "CARDIOLOGIST", noon, Severity::Minor), 3);  // D2 (1) + D3 (2)

    // Fill every cardiologist slot at H2.
    Doctor* first = reserve(manager, *heart, noon);
    CHECK(first != nullptr);
    Doctor* second = reserve(manager, *heart, noon);
    Doctor* third = reserve(manager, *heart, noon);
    CHECK(second != nullptr && third != nullptr);
    CHECK(reserve(manager, *heart, noon) == nullptr);
    CHECK_EQ(manager.patientsFittable(*heart, "CARDIOLOGIST", noon, Severity::Minor), 0);

    std::vector<HospitalCheck> checks =
        manager.checkAll(request, noon, travel, [](const std::string&) { return 3; });
    CHECK_EQ(checks[1].reason, std::string("every cardiologist is busy"));

    manager.scheduleRelease(first, 100.0, 1);
    manager.scheduleRelease(second, 50.0, 1);
    CHECK_EQ(manager.releaseDue(40.0), 0);
    CHECK_EQ(manager.releaseDue(60.0), 1);  // only the one due at 50
    CHECK_EQ(manager.patientsFittable(*heart, "CARDIOLOGIST", noon, Severity::Minor), 1);
    CHECK_EQ(manager.releaseDue(100.0), 1);
    CHECK_EQ(manager.patientsFittable(*heart, "CARDIOLOGIST", noon, Severity::Minor), 2);
}

static void testPooledUnits() {
    testkit::section("load units by severity, pooled across the doctors of one specialty");
    HospitalManager pool;
    std::string error;
    pool.addHospital("H9", "Pool Hospital", 9, {}, &error);
    pool.addDoctor(makeDoctor("P1", "H9", "ORTHOPEDIC_SURGEON", 0, 1440, 2), &error);
    pool.addDoctor(makeDoctor("P2", "H9", "ORTHOPEDIC_SURGEON", 0, 1440, 2), &error);
    Hospital* ortho = pool.find("H9");
    const std::string spec = "ORTHOPEDIC_SURGEON";
    int units = 0;

    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Minor), 4);     // 1 unit each
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Serious), 2);   // 2 units each
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Critical), 2);  // whole doctor each

    // A serious patient takes 2 units from one doctor.
    Doctor* serious = pool.reserveDoctor(*ortho, spec, 600, Severity::Serious, &units);
    CHECK(serious != nullptr && units == 2);
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Minor), 2);
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Serious), 1);
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Critical), 1);

    // A minor patient goes to the doctor with less load.
    Doctor* minor = pool.reserveDoctor(*ortho, spec, 600, Severity::Minor, &units);
    CHECK(minor != nullptr && minor != serious && units == 1);
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Minor), 1);
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Critical), 0);

    // CRITICAL needs a doctor with nothing else; none is free now.
    CHECK(pool.reserveDoctor(*ortho, spec, 600, Severity::Critical, &units) == nullptr);

    // Load comes back when treatment ends.
    pool.scheduleRelease(minor, 50.0, 1);
    CHECK_EQ(pool.releaseDue(60.0), 1);
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Minor), 2);
    pool.scheduleRelease(serious, 100.0, 2);
    CHECK_EQ(pool.releaseDue(100.0), 1);
    CHECK_EQ(pool.patientsFittable(*ortho, spec, 600, Severity::Critical), 2);
}

int main() {
    testSetup();
    testRanking();
    testCheckAllReasons();
    testCapacity();
    testPooledUnits();
    return testkit::summary("hospital");
}
