// Tests for the triage module. Build and run with: make test-triage
#include "../testkit/check.h"
#include "triage.h"

using namespace rahat;

// A small catalog built in code, so this module can be tested without data files.
static void buildCatalog(InjuryCatalog& catalog) {
    Injury* bleeding = catalog.addInjury("BLEEDING", "Bleeding injury", 5);
    bleeding->questions.push_back({"How much is it bleeding?",
                                   {{"Slow", 0}, {"Soaking cloth", 3}, {"Spurting", 6}}});
    bleeding->questions.push_back({"Is the patient conscious?",
                                   {{"Yes", 0}, {"Confused", 3}, {"Unconscious", 5}}});
    // Added out of order on purpose: addRule keeps them sorted.
    bleeding->addRule({Severity::Critical, 9, "GENERAL_SURGEON", {"BLOOD_BANK", "ICU"}, AmbulanceType::ALS});
    bleeding->addRule({Severity::Minor, 0, "EMERGENCY_PHYSICIAN", {}, AmbulanceType::BLS});
    bleeding->addRule({Severity::Serious, 5, "GENERAL_SURGEON", {"BLOOD_BANK"}, AmbulanceType::BLS});

    Injury* cardiac = catalog.addInjury("CARDIAC_ARREST", "Cardiac arrest", 10);
    cardiac->questions.push_back({"Is the patient breathing?",
                                  {{"Normally", 0}, {"Gasping", 5}, {"Not breathing", 8}}});
    cardiac->addRule({Severity::Serious, 0, "CARDIOLOGIST", {"ICU"}, AmbulanceType::ALS});
    cardiac->addRule({Severity::Critical, 5, "CARDIOLOGIST", {"ICU", "CATH_LAB"}, AmbulanceType::ALS});
}

static Request makeRequest(const std::string& id, const std::string& injury, std::vector<int> answers,
                           double receivedAt, long sequence, int patients = 1) {
    Request r;
    r.id = id;
    r.injuryCode = injury;
    r.answers = answers;
    r.receivedAt = receivedAt;
    r.sequence = sequence;
    r.patientCount = patients;
    return r;
}

static void testCatalog() {
    testkit::section("InjuryCatalog");
    InjuryCatalog catalog;
    buildCatalog(catalog);

    CHECK_EQ(catalog.all().size(), 2u);
    CHECK(catalog.addInjury("BLEEDING", "Duplicate", 1) == nullptr);
    CHECK(catalog.find("STROKE") == nullptr);

    const Injury* bleeding = catalog.find("BLEEDING");
    CHECK(bleeding != nullptr);
    CHECK_EQ(bleeding->rules.front().minPoints, 0);
    CHECK(bleeding->ruleFor(4)->severity == Severity::Minor);
    CHECK(bleeding->ruleFor(5)->severity == Severity::Serious);
    CHECK(bleeding->ruleFor(11)->severity == Severity::Critical);

    std::vector<std::string> problems;
    CHECK(catalog.validate(&problems));

    Injury* broken = catalog.addInjury("DOG_BITE", "Dog bite", 2);
    broken->addRule({Severity::Moderate, 2, "EMERGENCY_PHYSICIAN", {}, AmbulanceType::BLS});
    CHECK(!catalog.validate(&problems));
    CHECK_EQ(problems.size(), 2u);  // no questions + no rule at 0 points
}

static void testAssess() {
    testkit::section("Triage::assess");
    InjuryCatalog catalog;
    buildCatalog(catalog);
    Triage triage(catalog);
    std::string error;

    Request minor = makeRequest("r1", "BLEEDING", {0, 0}, 0, 1);
    CHECK(triage.assess(minor, &error));
    CHECK(minor.severity == Severity::Minor);
    CHECK_EQ(minor.priorityScore, 5);
    CHECK_EQ(minor.requiredSpecialization, std::string("EMERGENCY_PHYSICIAN"));

    Request critical = makeRequest("r2", "BLEEDING", {2, 1}, 0, 2);  // 6 + 3 = 9 points
    CHECK(triage.assess(critical, &error));
    CHECK(critical.severity == Severity::Critical);
    CHECK_EQ(critical.points, 9);
    CHECK_EQ(critical.priorityScore, 14);
    CHECK(critical.requiredAmbulance == AmbulanceType::ALS);
    CHECK_EQ(critical.requiredFacilities.size(), 2u);

    Request crowd = makeRequest("r3", "BLEEDING", {1, 0}, 0, 3, 9);  // 8 extra patients, bonus capped at 5
    CHECK(triage.assess(crowd, &error));
    CHECK_EQ(crowd.priorityScore, 5 + 3 + kMaxCrowdBonus);

    Request unknown = makeRequest("r4", "BROKEN_LEG", {}, 0, 4);
    CHECK(!triage.assess(unknown, &error));

    Request wrongCount = makeRequest("r5", "BLEEDING", {1}, 0, 5);
    CHECK(!triage.assess(wrongCount, &error));

    Request badOption = makeRequest("r6", "BLEEDING", {0, 7}, 0, 6);
    CHECK(!triage.assess(badOption, &error));
}

static void testQueueOrder() {
    testkit::section("RequestQueue order");
    InjuryCatalog catalog;
    buildCatalog(catalog);
    Triage triage(catalog);
    std::string error;

    // Case 2: several requests at once
    Request minorBleed = makeRequest("minor-bleed", "BLEEDING", {0, 0}, 10.0, 1);
    Request seriousBleed = makeRequest("serious-bleed", "BLEEDING", {1, 1}, 10.0, 2);   // 6 points, SERIOUS
    Request cardiacA = makeRequest("cardiac-early", "CARDIAC_ARREST", {1}, 10.0, 3);     // CRITICAL, score 15
    Request cardiacB = makeRequest("cardiac-late", "CARDIAC_ARREST", {1}, 11.0, 4);      // same, but later
    Request cardiacWorse = makeRequest("cardiac-worse", "CARDIAC_ARREST", {2}, 12.0, 5); // CRITICAL, score 18
    Request heartOk = makeRequest("cardiac-serious", "CARDIAC_ARREST", {0}, 9.0, 6);    // SERIOUS, score 10

    Request* all[] = {&minorBleed, &seriousBleed, &cardiacA, &cardiacB, &cardiacWorse, &heartOk};
    RequestQueue queue;
    for (Request* r : all) {
        CHECK(triage.assess(*r, &error));
        queue.push(r);
    }

    std::vector<Request*> preview = queue.ordered();
    CHECK_EQ(preview.size(), 6u);
    CHECK_EQ(queue.size(), 6);  // preview did not empty the queue

    CHECK_EQ(queue.pop()->id, std::string("cardiac-worse"));    // critical, highest score
    CHECK_EQ(queue.pop()->id, std::string("cardiac-early"));    // critical, same score, earlier
    CHECK_EQ(queue.pop()->id, std::string("cardiac-late"));
    CHECK_EQ(queue.pop()->id, std::string("serious-bleed"));    // serious, score 5 + 6 = 11
    CHECK_EQ(queue.pop()->id, std::string("cardiac-serious"));  // serious, score 10 (asked earlier, but lower score)
    CHECK_EQ(queue.pop()->id, std::string("minor-bleed"));
    CHECK(queue.empty());
}

int main() {
    testCatalog();
    testAssess();
    testQueueOrder();
    return testkit::summary("triage");
}
