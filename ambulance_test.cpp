// Tests for the ambulance module. Build and run with: make test-ambulance
#include "../testkit/check.h"
#include "ambulance.h"

using namespace rahat;

static DoublyLinkedList<Checkpoint> route(std::vector<Checkpoint> checkpoints) {
    DoublyLinkedList<Checkpoint> list;
    for (const Checkpoint& c : checkpoints) list.pushBack(c);
    return list;
}

static Request needing(AmbulanceType type) {
    Request r;
    r.id = "REQ";
    r.requiredAmbulance = type;
    return r;
}

static void testSelection() {
    testkit::section("Selection");
    AmbulanceManager fleet;
    std::string error;
    fleet.addAmbulance("A1", "H1", AmbulanceType::ALS, &error);
    fleet.addAmbulance("A2", "H1", AmbulanceType::BLS, &error);
    fleet.addAmbulance("A3", "H2", AmbulanceType::BLS, &error);
    CHECK(fleet.addAmbulance("A1", "H2", AmbulanceType::BLS, &error) == nullptr);

    CHECK_EQ(fleet.countFreeSuitable("H1", AmbulanceType::BLS), 2);  // ALS can do BLS work
    CHECK_EQ(fleet.countFreeSuitable("H1", AmbulanceType::ALS), 1);
    CHECK_EQ(fleet.countFreeSuitable("H2", AmbulanceType::ALS), 0);

    CHECK_EQ(fleet.selectAmbulance(needing(AmbulanceType::BLS), "H1")->id, std::string("A2"));  // keeps ALS free
    CHECK_EQ(fleet.selectAmbulance(needing(AmbulanceType::ALS), "H1")->id, std::string("A1"));
    CHECK(fleet.selectAmbulance(needing(AmbulanceType::ALS), "H2") == nullptr);

    fleet.find("A2")->status = AmbulanceStatus::ToScene;
    CHECK_EQ(fleet.selectAmbulance(needing(AmbulanceType::BLS), "H1")->id, std::string("A1"));  // ALS as fallback
}

static void testTrip() {
    testkit::section("Trip lifecycle");
    AmbulanceManager fleet;
    std::string error;
    Ambulance* a1 = fleet.addAmbulance("A1", "H1", AmbulanceType::ALS, &error);

    // Hospital(10) -> Chowk(20) -> Patient(30): 0, 4, 6 minutes
    DoublyLinkedList<Checkpoint> toScene = route({{10, "Hospital", 0}, {20, "Chowk", 4}, {30, "Patient", 6}});
    // Patient(30) -> Chowk(20) -> Hospital(10): 0, 2, 6 minutes
    DoublyLinkedList<Checkpoint> toHospital = route({{30, "Patient", 0}, {20, "Chowk", 2}, {10, "Hospital", 6}});

    Trip* trip = fleet.startTrip(a1, "REQ-1", "H1", toScene, toHospital, 100.0, 5.0, &error);
    CHECK(trip != nullptr);
    CHECK(a1->status == AmbulanceStatus::ToScene);
    CHECK_EQ(a1->requestId, std::string("REQ-1"));
    CHECK_EQ(trip->route.size(), 5);  // scene is not duplicated
    CHECK_NEAR(trip->arriveSceneAt, 106.0, 1e-9);
    CHECK_NEAR(trip->leaveSceneAt, 111.0, 1e-9);
    CHECK_NEAR(trip->arriveHospitalAt, 117.0, 1e-9);
    CHECK_EQ(trip->scene->value.name, std::string("Patient"));

    // Busy ambulance cannot start another trip.
    CHECK(fleet.startTrip(a1, "REQ-2", "H1", toScene, toHospital, 100.0, 5.0, &error) == nullptr);

    CHECK(fleet.update(103.0).empty());
    CHECK_EQ(trip->position->value.name, std::string("Hospital"));
    CHECK_EQ(fleet.update(104.5).size(), 0u);
    CHECK_EQ(trip->position->value.name, std::string("Chowk"));
    CHECK_EQ(trip->nextCheckpoint()->name, std::string("Patient"));

    std::vector<TripEvent> atScene = fleet.update(107.0);
    CHECK_EQ(atScene.size(), 1u);
    CHECK(atScene[0].type == TripEventType::ArrivedAtScene);
    CHECK_NEAR(atScene[0].time, 106.0, 1e-9);  // event carries the scheduled time
    CHECK(a1->status == AmbulanceStatus::AtScene);

    std::vector<TripEvent> leaving = fleet.update(112.0);
    CHECK_EQ(leaving.size(), 1u);
    CHECK(leaving[0].type == TripEventType::LeftScene);
    CHECK(a1->status == AmbulanceStatus::ToHospital);
    CHECK(fleet.tripOf("A1") != nullptr);

    std::vector<TripEvent> done = fleet.update(120.0);
    CHECK_EQ(done.size(), 1u);
    CHECK(done[0].type == TripEventType::ArrivedAtHospital);
    // Patient delivered; the ambulance waits for the hospital's confirmation.
    CHECK(a1->status == AmbulanceStatus::AtHospital);
    CHECK_EQ(a1->requestId, std::string("REQ-1"));
    CHECK_NEAR(a1->releaseAt, 175.0, 1e-9);  // dispatched at 100 + 75 min timeout
    CHECK(fleet.activeTrips().empty());
    CHECK(fleet.tripOf("A1") == nullptr);
    CHECK_EQ(fleet.countFreeSuitable("H1", AmbulanceType::BLS), 0);

    std::vector<TripEvent> confirm;
    CHECK(fleet.confirmReturn("A1", "H1", 125.0, &confirm, &error));
    CHECK_EQ(confirm.size(), 1u);
    CHECK(confirm[0].type == TripEventType::ReturnConfirmed);
    CHECK(a1->status == AmbulanceStatus::Available);
    CHECK(a1->requestId.empty());
}

static void testReturnTimeout() {
    testkit::section("Return timeout and early confirmation");
    AmbulanceManager fleet;
    std::string error;
    Ambulance* a = fleet.addAmbulance("A1", "H1", AmbulanceType::ALS, &error);
    DoublyLinkedList<Checkpoint> toScene = route({{10, "Hospital", 0}, {30, "Patient", 10}});
    DoublyLinkedList<Checkpoint> toHospital = route({{30, "Patient", 0}, {10, "Hospital", 10}});

    // Never confirmed: free 75 minutes after dispatch.
    fleet.startTrip(a, "R1", "H1", toScene, toHospital, 0.0, 5.0, &error);
    fleet.update(25.0);  // reached the hospital at 25
    CHECK(a->status == AmbulanceStatus::AtHospital);
    CHECK(fleet.update(74.0).empty());
    std::vector<TripEvent> late = fleet.update(80.0);
    CHECK_EQ(late.size(), 1u);
    CHECK(late[0].type == TripEventType::ReturnTimedOut);
    CHECK_NEAR(late[0].time, 75.0, 1e-9);
    CHECK(a->status == AmbulanceStatus::Available);

    // A long trip is never freed before it reaches the hospital.
    fleet.setReturnTimeout(20.0);
    fleet.startTrip(a, "R2", "H1", toScene, toHospital, 100.0, 5.0, &error);
    fleet.update(121.0);  // timeout passed, but still on the road
    CHECK(a->status == AmbulanceStatus::ToHospital);
    std::vector<TripEvent> back = fleet.update(125.0);  // arrives after the timeout: free at once
    CHECK_EQ(back.size(), 2u);
    CHECK(back[0].type == TripEventType::ArrivedAtHospital);
    CHECK(back[1].type == TripEventType::ReturnTimedOut);
    CHECK_NEAR(back[1].time, 125.0, 1e-9);
    CHECK(a->status == AmbulanceStatus::Available);

    // Confirmation rules.
    std::vector<TripEvent> events;
    fleet.startTrip(a, "R3", "H1", toScene, toHospital, 200.0, 5.0, &error);
    CHECK(!fleet.confirmReturn("A1", "H1", 201.0, &events, &error));  // going to the patient
    CHECK(!fleet.confirmReturn("A1", "H2", 216.0, &events, &error));  // wrong hospital
    fleet.update(216.0);                                              // on the way back
    CHECK(fleet.confirmReturn("A1", "H1", 218.0, &events, &error));
    CHECK_EQ(events.size(), 2u);  // the trip ends now: delivered, then confirmed
    CHECK(events[0].type == TripEventType::ArrivedAtHospital);
    CHECK(events[1].type == TripEventType::ReturnConfirmed);
    CHECK(fleet.activeTrips().empty());
    CHECK(!fleet.confirmReturn("A1", "H1", 219.0, &events, &error));  // already available
}

static void testTimeJump() {
    testkit::section("Big time jump");
    AmbulanceManager fleet;
    std::string error;
    Ambulance* a = fleet.addAmbulance("A9", "H1", AmbulanceType::BLS, &error);
    Ambulance* b = fleet.addAmbulance("A8", "H1", AmbulanceType::BLS, &error);

    // Patient at the hospital's own landmark: zero travel both ways.
    DoublyLinkedList<Checkpoint> here = route({{10, "Hospital", 0}});
    fleet.startTrip(a, "SLOW", "H1", route({{10, "Hospital", 0}, {30, "Patient", 8}}),
                    route({{30, "Patient", 0}, {10, "Hospital", 8}}), 0.0, 2.0, &error);
    Trip* instant = fleet.startTrip(b, "FAST", "H1", here, here, 0.0, 2.0, &error);
    CHECK_NEAR(instant->arriveHospitalAt, 2.0, 1e-9);

    std::vector<TripEvent> events = fleet.update(1000.0);  // everything finishes at once
    CHECK_EQ(events.size(), 8u);  // 3 trip events each, then both counted back by the timeout
    for (size_t i = 1; i < events.size(); ++i) CHECK(events[i - 1].time <= events[i].time);
    CHECK_EQ(events.front().requestId, std::string("FAST"));
    CHECK(fleet.activeTrips().empty());
    CHECK(a->status == AmbulanceStatus::Available && b->status == AmbulanceStatus::Available);
}

int main() {
    testSelection();
    testTrip();
    testTimeJump();
    testReturnTimeout();
    return testkit::summary("ambulance");
}
