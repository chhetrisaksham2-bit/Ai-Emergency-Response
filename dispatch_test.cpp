// Tests for the dispatch engine: the three cases, automatic assignment with
// reasons, the batch window, and the ambulance return button / timeout.
#include "../testkit/check.h"
#include "dispatch.h"

using namespace rahat;

// A tiny world built in code (all roads clear, 60 km/h, so km = minutes):
//
//   H1 North (1) --3-- Centre (2) --2-- Patient Street (4)
//                        |                  |
//                        4                 4.5
//                        |                  |
//                  H2 South (3) ----------- +
//                        |
//                       10
//                        |
//                  H3 Far Village (5)
//
// From Patient Street: H2 4.5 min, H1 5 min, H3 14.5 min.
struct TestWorld {
    CityMap map;
    InjuryCatalog injuries;
    FirstAidGuide firstAid;
    HospitalManager hospitals;
    AmbulanceManager ambulances;

    TestWorld() {
        std::string e;
        map.addLandmark(1, "H1 North", &e);
        map.addLandmark(2, "Centre", &e);
        map.addLandmark(3, "H2 South", &e);
        map.addLandmark(4, "Patient Street", &e);
        map.addLandmark(5, "Far Village", &e);
        map.addRoad(1, 2, {"R1", 3, 60, TrafficLevel::Clear, ""}, &e);
        map.addRoad(2, 4, {"R2", 2, 60, TrafficLevel::Clear, ""}, &e);
        map.addRoad(2, 3, {"R3", 4, 60, TrafficLevel::Clear, ""}, &e);
        map.addRoad(3, 4, {"R4", 4.5, 60, TrafficLevel::Clear, ""}, &e);
        map.addRoad(3, 5, {"R5", 10, 60, TrafficLevel::Clear, ""}, &e);

        Injury* cardiac = injuries.addInjury("CARDIAC", "Cardiac arrest", 10);
        cardiac->questions.push_back({"Breathing?", {{"Normal", 0}, {"Gasping", 5}, {"Not breathing", 8}}});
        cardiac->addRule({Severity::Serious, 0, "CARDIOLOGIST", {"ICU"}, AmbulanceType::ALS});
        cardiac->addRule({Severity::Critical, 8, "CARDIOLOGIST", {"ICU", "CATH_LAB"}, AmbulanceType::ALS});

        Injury* cut = injuries.addInjury("CUT", "Cut", 2);
        cut->questions.push_back({"Depth?", {{"Small", 0}, {"Deep", 4}}});
        cut->addRule({Severity::Minor, 0, "EMERGENCY_PHYSICIAN", {}, AmbulanceType::BLS});

        firstAid.addStep("CARDIAC", "ANY", "Attach defibrillator pads");
        firstAid.addStep("CARDIAC", "CRITICAL", "Start CPR");
        firstAid.addStep("CUT", "ANY", "Apply pressure");

        hospitals.addHospital("H1", "North Hospital", 1, {"ICU", "CATH_LAB"}, &e);
        hospitals.addHospital("H2", "South Hospital", 3, {"ICU"}, &e);
        hospitals.addHospital("H3", "Far Hospital", 5, {"ICU", "CATH_LAB"}, &e);
        addDoctor("D1", "H1", "CARDIOLOGIST", 1);
        addDoctor("D2", "H1", "EMERGENCY_PHYSICIAN", 5);
        addDoctor("D3", "H2", "CARDIOLOGIST", 4);  // 4 load units: two SERIOUS patients
        addDoctor("D4", "H2", "EMERGENCY_PHYSICIAN", 5);
        addDoctor("D5", "H3", "CARDIOLOGIST", 3);
        addDoctor("D6", "H3", "EMERGENCY_PHYSICIAN", 5);

        addAmbulance("A1", "H1", AmbulanceType::ALS);
        addAmbulance("A2", "H1", AmbulanceType::BLS);
        addAmbulance("A3", "H2", AmbulanceType::ALS);
        addAmbulance("A4", "H3", AmbulanceType::ALS);
        addAmbulance("A5", "H3", AmbulanceType::ALS);
        addAmbulance("A6", "H3", AmbulanceType::BLS);
    }

    void addDoctor(const std::string& id, const std::string& hospital, const std::string& spec, int maxPatients) {
        Doctor d;
        d.id = id;
        d.hospitalId = hospital;
        d.name = "Dr. " + id;
        d.specialization = spec;
        d.shiftStart = 0;
        d.shiftEnd = 1440;
        d.maxPatients = maxPatients;
        std::string e;
        hospitals.addDoctor(d, &e);
    }

    void addAmbulance(const std::string& id, const std::string& hospital, AmbulanceType type) {
        std::string e;
        hospitals.linkAmbulance(ambulances.addAmbulance(id, hospital, type, &e), &e);
    }
};

static Request* makeRequest(const std::string& id, const std::string& injury, int answer, int patients = 1,
                            int landmark = 4) {
    Request* r = new Request();
    r->id = id;
    r->injuryCode = injury;
    r->answers = {answer};
    r->landmarkId = landmark;
    r->patientCount = patients;
    return r;
}

static int countEvents(const std::vector<EngineEvent>& events, EventType type, const std::string& requestId = "") {
    int count = 0;
    for (const EngineEvent& e : events) {
        if (e.type == type && (requestId.empty() || e.requestId == requestId)) ++count;
    }
    return count;
}

static bool hasLine(const std::vector<std::string>& lines, const std::string& part) {
    for (const std::string& line : lines) {
        if (line.find(part) != std::string::npos) return true;
    }
    return false;
}

static EngineSettings noBatch() {
    EngineSettings settings;
    settings.batchMinutes = 0;
    return settings;
}

static void testCase1SingleRequest() {
    testkit::section("Case 1: one request, full trip, automatic assignment");
    TestWorld w;
    DispatchEngine engine(w.map, w.injuries, w.firstAid, w.hospitals, w.ambulances, noBatch());
    std::string error;

    CHECK(engine.submit(makeRequest("ONE", "CARDIAC", 0), 600.0, &error));  // SERIOUS: ICU + cardiologist
    engine.tick(600.0);
    std::vector<EngineEvent> events = engine.takeEvents();

    CHECK_EQ(countEvents(events, EventType::Received), 1);
    CHECK_EQ(countEvents(events, EventType::HospitalsChecked), 1);
    CHECK_EQ(countEvents(events, EventType::Dispatched), 1);
    CHECK_EQ(countEvents(events, EventType::FirstAid), 1);

    const Request* request = engine.findRequest("ONE");
    CHECK(request->status == RequestStatus::InProgress);
    std::vector<Assignment> assignments = engine.assignmentsFor("ONE");
    CHECK_EQ(assignments.size(), 1u);
    CHECK_EQ(assignments[0].hospitalId, std::string("H2"));  // nearest with ICU + cardiologist
    CHECK_EQ(assignments[0].ambulanceId, std::string("A3"));
    CHECK_NEAR(assignments[0].arriveSceneAt, 604.5, 1e-9);
    CHECK_NEAR(assignments[0].arriveHospitalAt, 614.0, 1e-9);  // 4.5 + 5 at scene + 4.5

    // Every hospital is listed with its verdict.
    std::vector<std::string> lines = engine.hospitalCheckLines("ONE");
    CHECK_EQ(lines.size(), 3u);
    CHECK(hasLine(lines, "South Hospital (5 min away) - SELECTED"));
    CHECK(hasLine(lines, "North Hospital (5 min away) - not selected: every cardiologist is busy"));  // D1 has 1 unit, SERIOUS needs 2

    for (const EngineEvent& e : events) {
        if (e.type == EventType::FirstAid) {
            CHECK(!e.forPatient);
            CHECK_EQ(e.details.size(), 1u);  // SERIOUS gets only the ANY step
            CHECK_EQ(e.hospitalId, std::string("H2"));
        }
        if (e.type == EventType::Dispatched) {
            CHECK_EQ(e.hospitalId, std::string("H2"));
            CHECK_EQ(e.ambulanceId, std::string("A3"));
            CHECK(e.text.find("Destination: South Hospital") != std::string::npos);
        }
    }

    engine.tick(605.0);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::ArrivedAtScene), 1);
    engine.tick(610.0);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::LeftScene), 1);
    engine.tick(614.0);
    events = engine.takeEvents();
    CHECK_EQ(countEvents(events, EventType::ArrivedAtHospital), 1);
    CHECK_EQ(countEvents(events, EventType::Completed), 1);
    CHECK(request->status == RequestStatus::Completed);
    // Patient delivered, but the ambulance waits for the hospital's return button.
    CHECK(w.ambulances.find("A3")->status == AmbulanceStatus::AtHospital);
    CHECK_EQ(engine.completedCount(), 1);
    CHECK(engine.activeRequests().empty());

    CHECK(engine.confirmReturn("H2", "A3", 616.0, &error));
    CHECK(w.ambulances.find("A3")->status == AmbulanceStatus::Available);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::AmbulanceReturned), 1);

    // The cardiologist stays busy for the treatment time, then is free again.
    Hospital* south = w.hospitals.find("H2");
    CHECK_EQ(w.hospitals.patientsFittable(*south, "CARDIOLOGIST", 600, Severity::Serious), 1);
    engine.tick(674.0);
    CHECK_EQ(w.hospitals.patientsFittable(*south, "CARDIOLOGIST", 600, Severity::Serious), 2);
}

static void testCase2Priority() {
    testkit::section("Case 2: many requests, most urgent first, skip reasons");
    TestWorld w;
    DispatchEngine engine(w.map, w.injuries, w.firstAid, w.hospitals, w.ambulances, noBatch());
    std::string error;

    // Submitted least urgent first; the queue must reorder them.
    CHECK(engine.submit(makeRequest("CUT", "CUT", 0), 600.0, &error));
    CHECK(engine.submit(makeRequest("SERIOUS", "CARDIAC", 0), 600.0, &error));
    CHECK(engine.submit(makeRequest("CRITICAL", "CARDIAC", 2), 600.0, &error));

    std::vector<const Request*> waiting = engine.activeRequests();
    CHECK_EQ(waiting.size(), 3u);
    CHECK_EQ(waiting[0]->id, std::string("CRITICAL"));
    CHECK_EQ(waiting[2]->id, std::string("CUT"));

    engine.tick(600.0);
    std::vector<EngineEvent> events = engine.takeEvents();

    std::vector<std::string> dispatchOrder;
    for (const EngineEvent& e : events) {
        if (e.type == EventType::Dispatched) dispatchOrder.push_back(e.requestId);
    }
    std::vector<std::string> expected = {"CRITICAL", "SERIOUS", "CUT"};
    CHECK(dispatchOrder == expected);

    // CRITICAL needs a cath lab -> H1 (nearest with one), ALS ambulance A1.
    CHECK_EQ(engine.assignmentsFor("CRITICAL")[0].hospitalId, std::string("H1"));
    CHECK_EQ(engine.assignmentsFor("CRITICAL")[0].ambulanceId, std::string("A1"));
    CHECK(hasLine(engine.hospitalCheckLines("CRITICAL"), "South Hospital (5 min away) - not selected: no cath lab"));
    // SERIOUS -> H2 with A3.
    CHECK_EQ(engine.assignmentsFor("SERIOUS")[0].ambulanceId, std::string("A3"));
    // CUT: H2 is nearest but its only ambulance just left, so it is skipped and H1 sends BLS A2.
    CHECK(hasLine(engine.hospitalCheckLines("CUT"),
                  "South Hospital (5 min away) - not selected: no BLS ambulance available right now"));
    CHECK_EQ(engine.assignmentsFor("CUT")[0].hospitalId, std::string("H1"));
    CHECK_EQ(engine.assignmentsFor("CUT")[0].ambulanceId, std::string("A2"));

    // Another critical patient: H1's only cardiologist is busy -> skipped -> Far Hospital.
    CHECK(engine.submit(makeRequest("CRITICAL-2", "CARDIAC", 2), 601.0, &error));
    engine.tick(601.0);
    engine.takeEvents();
    CHECK(hasLine(engine.hospitalCheckLines("CRITICAL-2"), "North Hospital (5 min away) - not selected: every cardiologist is busy"));
    CHECK_EQ(engine.assignmentsFor("CRITICAL-2")[0].hospitalId, std::string("H3"));
}

static void testBatchWindow() {
    testkit::section("Batch window: requests close together are served by priority");
    TestWorld w;
    EngineSettings settings;
    settings.batchMinutes = 1.0;
    DispatchEngine engine(w.map, w.injuries, w.firstAid, w.hospitals, w.ambulances, settings);
    std::string error;
    double servesAt = 0;

    CHECK(engine.submit(makeRequest("CUT", "CUT", 0), 600.0, &error));
    engine.tick(600.0);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::Dispatched), 0);  // still collecting
    CHECK(engine.batchWaiting(600.0, &servesAt));
    CHECK_NEAR(servesAt, 601.0, 1e-9);

    CHECK(engine.submit(makeRequest("SERIOUS", "CARDIAC", 0), 600.5, &error));  // arrives later
    engine.tick(600.5);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::Dispatched), 0);

    engine.tick(601.0);
    std::vector<std::string> order;
    for (const EngineEvent& e : engine.takeEvents()) {
        if (e.type == EventType::Dispatched) order.push_back(e.requestId);
    }
    std::vector<std::string> expected = {"SERIOUS", "CUT"};
    CHECK(order == expected);
    CHECK(!engine.batchWaiting(601.0, &servesAt));

    // A CRITICAL request does not wait for the window.
    CHECK(engine.submit(makeRequest("NOW", "CARDIAC", 2), 610.0, &error));
    engine.tick(610.0);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::Dispatched, "NOW"), 1);
}

static void testReturnButtonAndTimeout() {
    testkit::section("Ambulance return: button, early return, timeout");
    TestWorld w;
    DispatchEngine engine(w.map, w.injuries, w.firstAid, w.hospitals, w.ambulances, noBatch());
    std::string error;

    CHECK(engine.submit(makeRequest("A", "CARDIAC", 0), 600.0, &error));  // A3 from H2, back at 614
    engine.tick(600.0);
    engine.takeEvents();

    CHECK(!engine.confirmReturn("H2", "A3", 601.0, &error));  // still going to the patient
    CHECK(error.find("not picked up") != std::string::npos);
    CHECK(!engine.confirmReturn("H1", "A3", 611.0, &error));  // wrong hospital
    CHECK(!engine.confirmReturn("H2", "A9", 611.0, &error));  // unknown ambulance

    // Pressed while the ambulance is on its way back: the trip ends now.
    engine.tick(610.0);  // left the scene
    engine.takeEvents();
    CHECK(engine.confirmReturn("H2", "A3", 612.0, &error));
    std::vector<EngineEvent> events = engine.takeEvents();
    CHECK_EQ(countEvents(events, EventType::ArrivedAtHospital), 1);
    CHECK_EQ(countEvents(events, EventType::Completed), 1);
    CHECK_EQ(countEvents(events, EventType::AmbulanceReturned), 1);
    CHECK(w.ambulances.find("A3")->status == AmbulanceStatus::Available);
    CHECK(!engine.confirmReturn("H2", "A3", 613.0, &error));  // already available

    // Nobody presses the button: counted back 1 h 15 min after dispatch.
    CHECK(engine.submit(makeRequest("B", "CARDIAC", 0), 620.0, &error));  // A3 again
    engine.tick(620.0);
    engine.tick(634.0);  // reached the hospital
    engine.takeEvents();
    CHECK(w.ambulances.find("A3")->status == AmbulanceStatus::AtHospital);
    engine.tick(694.9);
    CHECK(w.ambulances.find("A3")->status == AmbulanceStatus::AtHospital);
    engine.tick(695.0);
    events = engine.takeEvents();
    CHECK(w.ambulances.find("A3")->status == AmbulanceStatus::Available);
    CHECK_EQ(countEvents(events, EventType::AmbulanceReturned), 1);
    for (const EngineEvent& e : events) {
        if (e.type == EventType::AmbulanceReturned) {
            CHECK(!e.forPatient);
            CHECK(e.text.find("automatically") != std::string::npos);
            CHECK(e.text.find("1 h 15 min") != std::string::npos);
        }
    }
}

static void testCase3ManyPatients() {
    testkit::section("Case 3: many patients at one place");
    TestWorld w;
    DispatchEngine engine(w.map, w.injuries, w.firstAid, w.hospitals, w.ambulances, noBatch());
    std::string error;

    // 10 patients, 6 ambulances in the whole city.
    CHECK(engine.submit(makeRequest("BUS", "CUT", 1, 10), 600.0, &error));
    engine.tick(600.0);
    engine.takeEvents();

    const Request* bus = engine.findRequest("BUS");
    CHECK_EQ(bus->patientsAssigned, 6);  // H2: 1, H1: 2, H3: 3
    CHECK(bus->status == RequestStatus::Waiting);
    std::vector<Assignment> assignments = engine.assignmentsFor("BUS");
    int toH1 = 0, toH2 = 0, toH3 = 0;
    for (const Assignment& a : assignments) {
        if (a.hospitalId == "H1") ++toH1;
        if (a.hospitalId == "H2") ++toH2;
        if (a.hospitalId == "H3") ++toH3;
    }
    CHECK_EQ(toH1, 2);
    CHECK_EQ(toH2, 1);
    CHECK_EQ(toH3, 3);

    // First retry: every hospital is out of ambulances, and that list is announced once...
    engine.tick(602.5);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::HospitalsChecked), 1);
    CHECK(hasLine(engine.hospitalCheckLines("BUS"), "no BLS ambulance available right now"));
    // ...later retries with the same result do not repeat it.
    engine.tick(605.0);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::HospitalsChecked), 0);

    // A3 is back at H2 at 614 and the hospital presses the button -> served immediately.
    engine.tick(614.0);
    CHECK(engine.confirmReturn("H2", "A3", 614.0, &error));
    engine.tick(614.0);
    std::vector<EngineEvent> events = engine.takeEvents();
    CHECK_EQ(countEvents(events, EventType::AmbulanceReturned), 1);
    CHECK_EQ(countEvents(events, EventType::Dispatched), 1);
    CHECK_EQ(bus->patientsAssigned, 7);

    // The others are never confirmed: the return timeout frees them and everyone gets a ride.
    for (double t = 615; t <= 900; t += 1.0) engine.tick(t);
    engine.takeEvents();
    CHECK_EQ(bus->patientsDelivered, 10);
    CHECK(bus->status == RequestStatus::Completed);
}

static void testInvalidRequests() {
    testkit::section("Invalid requests");
    TestWorld w;
    DispatchEngine engine(w.map, w.injuries, w.firstAid, w.hospitals, w.ambulances, noBatch());
    std::string error;

    Request* badLandmark = makeRequest("BAD1", "CUT", 0, 1, 99);
    CHECK(!engine.submit(badLandmark, 600.0, &error));
    delete badLandmark;

    Request* badAnswer = makeRequest("BAD2", "CUT", 7);
    CHECK(!engine.submit(badAnswer, 600.0, &error));
    delete badAnswer;

    CHECK(engine.submit(makeRequest("DUP", "CUT", 0), 600.0, &error));
    Request* duplicate = makeRequest("DUP", "CUT", 0);
    CHECK(!engine.submit(duplicate, 600.0, &error));
    delete duplicate;

    // Nobody treats this specialisation -> waiting, with a clear reason.
    Injury* burn = w.injuries.addInjury("BURN", "Burn", 4);
    burn->questions.push_back({"Size?", {{"Small", 0}, {"Large", 5}}});
    burn->addRule({Severity::Serious, 0, "BURN_SPECIALIST", {}, AmbulanceType::BLS});
    CHECK(engine.submit(makeRequest("BURN", "BURN", 1), 600.0, &error));
    engine.tick(600.0);
    CHECK_EQ(countEvents(engine.takeEvents(), EventType::Waiting, "BURN"), 1);
    CHECK(engine.findRequest("BURN")->status == RequestStatus::Waiting);
    CHECK(hasLine(engine.hospitalCheckLines("BURN"), "not selected: no burn specialist on duty"));
}

int main() {
    testCase1SingleRequest();
    testCase2Priority();
    testBatchWindow();
    testReturnButtonAndTimeout();
    testCase3ManyPatients();
    testInvalidRequests();
    return testkit::summary("dispatch");
}
