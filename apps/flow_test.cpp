// End-to-end test of the Phase 3 windows without a screen:
// generator / patient -> admin -> hospital -> admin -> patient, through real files.
//
//   ./build/flow_test [data folder]
#include <filesystem>
#include <fstream>

#include "../modules/testkit/check.h"
#include "admin_server.h"
#include "generator.h"
#include "hospital_client.h"
#include "patient_client.h"
#include "protocol.h"

using namespace rahat;
namespace fs = std::filesystem;

static std::string dataDir = "data";

static int countType(const PatientClient& p, const std::string& type) {
    int n = 0;
    for (const PatientUpdate& u : p.updates()) n += u.type == type ? 1 : 0;
    return n;
}

static bool loadData(World& world) {
    LoadReport report;
    bool ok = loadWorld(dataDir, world, report);
    for (const std::string& e : report.errors) std::cout << "  data error: " << e << "\n";
    return ok;
}

static void testScenarioFile() {
    testkit::section("Scenario file");
    World world;
    CHECK(loadData(world));
    fs::path file = fs::temp_directory_path() / "rahat_scenario_test.csv";
    {
        std::ofstream out(file);
        out << "# comment line\n"
               "send_at,landmark,injury,patients,answers\n"
               "0,clock tower,Burns,1,\"Several body parts|Face, neck or private parts|Yes|Fire\"\n"
               "2,6,FRACTURE,2,2|1|1\n"
               "0,Nowhere,BURNS,1,1|1|1|1\n"
               "0,ISBT,STROKE,1,Yes\n"
               "0,ISBT,FRACTURE,1,1|1|Maybe\n";
    }
    std::vector<ScenarioItem> items;
    std::vector<std::string> problems;
    CHECK(!loadScenario(file.string(), world, &items, &problems));
    CHECK_EQ(problems.size(), 3u);  // unknown landmark, wrong answer count, unknown answer
    CHECK_EQ(items.size(), 2u);
    if (items.size() == 2) {
        CHECK_EQ(items[0].label, std::string("Burns at Clock Tower"));  // sorted by send time, names any case
        CHECK_EQ(items[0].request.answers.size(), 4u);
        CHECK_EQ(items[0].request.answers[0], 2);
        CHECK_EQ(items[1].request.patientCount, 2);
        CHECK_EQ(items[1].request.answers[0], 1);  // "2" = second option
        CHECK_EQ(items[1].label, std::string("Fracture / broken bone at Dilaram Chowk (2 patients)"));
    }

    // An answer with a comma must be in double quotes, or the row has too many columns.
    {
        std::ofstream out(file);
        out << "send_at,landmark,injury,patients,answers\n"
               "0,ISBT,MUSCLE_TEAR,1,Mild, can still move|None|No\n";
    }
    items.clear();
    problems.clear();
    CHECK(!loadScenario(file.string(), world, &items, &problems));
    CHECK_EQ(problems.size(), 1u);
    if (!problems.empty()) CHECK(problems[0].find("expected 5 values") != std::string::npos);
    fs::remove(file);

    // Random mode: the same seed gives the same requests.
    std::mt19937 a(7), b(7);
    for (int i = 0; i < 5; ++i) {
        ScenarioItem x = randomItem(world, a, i), y = randomItem(world, b, i);
        CHECK_EQ(x.label, y.label);
        CHECK(x.request.answers == y.request.answers);
    }
}

static void testFullFlow() {
    testkit::section("Full flow: three windows talking through files");
    fs::path shared = fs::temp_directory_path() / "rahat_flow_test";
    fs::remove_all(shared);

    World world;
    CHECK(loadData(world));
    EngineSettings settings;  // default: 1 minute batch, 75 minute return timeout
    AdminServer admin(world, shared.string(), settings);

    PatientClient early(shared.string());
    Request request;
    std::string id, error;
    request.injuryCode = "MUSCLE_TEAR";
    request.answers = {0, 0, 0};
    request.landmarkId = 4;
    CHECK(!early.send(request, &id, &error));  // admin not running yet
    CHECK(error.find("admin") != std::string::npos);

    CHECK(admin.start(&error));

    // Three patients, least urgent first, in the same batch.
    PatientClient minor(shared.string()), moderate(shared.string()), serious(shared.string());
    Request m = request;                                     // MINOR muscle tear at ISBT
    Request f;
    f.injuryCode = "FRACTURE";
    f.answers = {1, 1, 0};                                   // arm, badly bent -> 5 points MODERATE
    f.landmarkId = 9;
    Request b;
    b.injuryCode = "BURNS";
    b.answers = {2, 1, 0, 1};                                // several parts, hands, fire -> 10 SERIOUS
    b.landmarkId = 1;
    std::string minorId, moderateId, seriousId;
    CHECK(minor.send(m, &minorId, &error));
    CHECK(moderate.send(f, &moderateId, &error));
    CHECK(serious.send(b, &seriousId, &error));

    double now = 600;
    CHECK(admin.step(now));
    AdminView view = admin.view(now);
    CHECK_EQ(view.active.size(), 3u);
    CHECK(!view.batch.empty());  // still collecting
    if (view.active.size() == 3) {
        CHECK_EQ(view.active[0].id, seriousId);  // queue shows the most urgent first
        CHECK_EQ(view.active[2].id, minorId);
    }
    minor.poll();
    CHECK_EQ(countType(minor, "RECEIVED"), 1);
    CHECK_EQ(countType(minor, "DISPATCHED"), 0);
    CHECK_EQ(minor.severity(), std::string("MINOR"));

    now = 601;  // batch window over: served together, most urgent first
    admin.step(now);
    for (PatientClient* p : {&minor, &moderate, &serious}) {
        p->poll();
        CHECK_EQ(countType(*p, "HOSPITALS_CHECKED"), 1);
        CHECK_EQ(countType(*p, "DISPATCHED"), 1);
        CHECK(!p->destination().empty());
        CHECK_EQ(p->hospitalsChecked().size(), world.hospitals.all().size());  // every hospital is listed
        bool selected = false;
        for (const std::string& line : p->hospitalsChecked()) selected = selected || line.find("SELECTED") != std::string::npos;
        CHECK(selected);
    }
    // The order of dispatch follows the priority, not the arrival.
    std::vector<std::string> order;
    for (const std::string& line : admin.view(now).log) {
        if (line.find("dispatched from") == std::string::npos) continue;
        if (line.find(protocol::shortId(seriousId)) != std::string::npos) order.push_back("serious");
        if (line.find(protocol::shortId(moderateId)) != std::string::npos) order.push_back("moderate");
        if (line.find(protocol::shortId(minorId)) != std::string::npos) order.push_back("minor");
    }
    std::vector<std::string> expected = {"serious", "moderate", "minor"};
    CHECK(order == expected);

    // The hospital that got the burns patient sees it, with the pickup point.
    std::vector<Assignment> burns = admin.dispatch().assignmentsFor(seriousId);
    CHECK_EQ(burns.size(), 1u);
    if (burns.empty()) return;
    std::string hospitalId = burns[0].hospitalId;
    std::string ambulanceId = burns[0].ambulanceId;
    HospitalClient hospital(shared.string(), hospitalId);
    CHECK(hospital.poll());
    CHECK(hospital.connected());
    CHECK_EQ(hospital.name(), world.hospitals.find(hospitalId)->name);
    bool found = false;
    for (const HospitalAssignment& a : hospital.assignments()) {
        if (a.requestId == seriousId) {
            found = true;
            CHECK_EQ(a.pickup, std::string("Clock Tower"));
            CHECK_EQ(a.destination, hospital.name());
            CHECK_EQ(a.ambulanceId, ambulanceId);
            CHECK_EQ(a.severity, std::string("SERIOUS"));
            CHECK(!a.firstAid.empty());
            CHECK(!a.route.empty());
        }
        CHECK(a.requestId == seriousId || a.requestId == moderateId || a.requestId == minorId);
    }
    CHECK(found);
    int freeBefore = hospital.freeAmbulances();
    CHECK(freeBefore < static_cast<int>(hospital.ambulances().size()));

    // Another hospital never sees this request.
    for (const Hospital* other : world.hospitals.all()) {
        if (other->id == hospitalId) continue;
        HospitalClient window(shared.string(), other->id);
        window.poll();
        for (const HospitalAssignment& a : window.assignments()) CHECK(a.requestId != seriousId);
    }

    // Pressing the button too early is refused.
    CHECK(hospital.pressReturned(ambulanceId, &error));
    admin.step(602);
    hospital.poll();
    CHECK(!hospital.lastResultOk());
    CHECK(hospital.lastResult().find("not picked up") != std::string::npos);

    // Let the trip run until the patient is delivered, then confirm the return.
    for (now = 603; now <= 700 && !serious.finished(); now += 1) {
        admin.step(now);
        serious.poll();
    }
    CHECK(serious.finished());
    CHECK_EQ(countType(serious, "COMPLETED"), 1);
    hospital.poll();
    CHECK_EQ(hospital.freeAmbulances(), freeBefore);  // still away until the button is pressed
    CHECK(hospital.pressReturned(ambulanceId, &error));
    admin.step(now);
    hospital.poll();
    CHECK(hospital.lastResultOk());
    CHECK_EQ(hospital.freeAmbulances(), freeBefore + 1);
    for (const HospitalAssignment& a : hospital.assignments()) {
        if (a.requestId == seriousId) {
            CHECK(a.returned);
            CHECK(a.timeline.size() >= 5u);  // dispatched, at scene, left, at hospital, returned
        }
    }

    // The other ambulances are never confirmed: counted back 1 h 15 min after dispatch (601 + 75).
    for (; now < 675.5; now += 1) admin.step(now);
    admin.step(676);
    for (const Ambulance* a : world.ambulances.all()) CHECK(a->status == AmbulanceStatus::Available);

    // A new admin session: the patient window notices its request is gone.
    PatientClient late(shared.string());
    CHECK(late.send(m, &id, &error));
    AdminServer restarted(world, shared.string(), settings);
    CHECK(restarted.start(&error));
    late.poll();
    CHECK(late.lost());

    fs::remove_all(shared);
}

static void testFastTrack() {
    testkit::section("Fast track: one question, EXTREME goes straight to the critical queue");
    fs::path shared = fs::temp_directory_path() / "rahat_fast_test";
    fs::remove_all(shared);
    World world;
    CHECK(loadData(world));
    CHECK_EQ(world.injuries.all()[0]->code, std::string("FAST_TRACK"));  // first in the patient's menu
    CHECK_EQ(world.injuries.find("FAST_TRACK")->questions.size(), 1u);
    CHECK_EQ(world.injuries.find("FAST_TRACK")->questions[0].options.size(), 5u);

    AdminServer admin(world, shared.string(), EngineSettings());  // 1 minute batch window
    std::string error, id, otherId;
    CHECK(admin.start(&error));

    PatientClient stable(shared.string()), extreme(shared.string());
    Request r;
    r.injuryCode = "FAST_TRACK";
    r.landmarkId = 5;
    r.answers = {1};  // "in pain or unwell, but stable" -> MODERATE, waits for the batch
    CHECK(stable.send(r, &otherId, &error));
    r.answers = {4};  // "EXTREME" -> CRITICAL, served at once
    CHECK(extreme.send(r, &id, &error));

    admin.step(600);
    extreme.poll();
    stable.poll();
    CHECK_EQ(extreme.severity(), std::string("CRITICAL"));
    CHECK_EQ(countType(extreme, "DISPATCHED"), 1);
    CHECK_EQ(stable.severity(), std::string("MODERATE"));
    CHECK_EQ(countType(stable, "DISPATCHED"), 1);  // a CRITICAL request ends the batch for everyone waiting
    fs::remove_all(shared);
}

int main(int argc, char** argv) {
    if (argc > 1) dataDir = argv[1];
    testScenarioFile();
    testFullFlow();
    testFastTrack();
    return testkit::summary("flow");
}
