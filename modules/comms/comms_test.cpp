// Tests for the comms module. Build and run with: make test-comms
#include <filesystem>
#include <fstream>

#include "../testkit/check.h"
#include "comms.h"

using namespace rahat;
namespace fs = std::filesystem;

static void testMessage() {
    testkit::section("Message");
    Message message;
    message.set("type", "FIRST_AID");
    message.set("text", "1. Apply pressure\n2. Keep warm = important\\really");
    message.set("count", 3);
    message.set("type", "DISPATCHED");  // replacing keeps the original key order

    std::string text = message.serialize();
    CHECK_EQ(text.find("type=DISPATCHED"), 0u);
    CHECK(text.find("\\n") != std::string::npos);  // newline escaped on disk

    Message parsed;
    std::string error;
    CHECK(Message::parse(text, &parsed, &error));
    CHECK_EQ(parsed.get("text"), message.get("text"));
    CHECK_EQ(parsed.getInt("count", -1), 3);
    CHECK_EQ(parsed.getInt("type", -1), -1);  // not a number
    CHECK_EQ(parsed.get("missing", "fallback"), std::string("fallback"));

    Message bad;
    CHECK(!Message::parse("no equals sign here", &bad, &error));
}

static void testRequestConversion() {
    testkit::section("Request <-> Message");
    Request original;
    original.id = "R123-001";
    original.injuryCode = "SNAKE_BITE";
    original.answers = {2, 0, 1};
    original.landmarkId = 14;
    original.patientCount = 3;

    Request copy;
    std::string error;
    CHECK(messageToRequest(requestToMessage(original), &copy, &error));
    CHECK_EQ(copy.id, original.id);
    CHECK_EQ(copy.injuryCode, original.injuryCode);
    CHECK(copy.answers == original.answers);
    CHECK_EQ(copy.landmarkId, 14);
    CHECK_EQ(copy.patientCount, 3);

    Message broken = requestToMessage(original);
    broken.set("landmark", "fourteen");
    CHECK(!messageToRequest(broken, &copy, &error));

    Message badAnswers = requestToMessage(original);
    badAnswers.set("answers", "1,x");
    CHECK(!messageToRequest(badAnswers, &copy, &error));

    Message missing;
    missing.set("id", "R1");
    CHECK(!messageToRequest(missing, &copy, &error));

    std::string first = makeRequestId();
    std::string second = makeRequestId();
    CHECK(first[0] == 'R');
    CHECK(first != second);
}

static void testChannel() {
    testkit::section("Channel: patient / generator <-> admin");
    fs::path shared = fs::temp_directory_path() / "rahat_comms_test";
    fs::remove_all(shared);

    Channel adminSide(shared.string());
    Channel patientSide(shared.string());
    std::string error;
    std::string session;
    CHECK(patientSide.currentSession().empty());  // admin not started yet
    CHECK(adminSide.startSession({"H1", "H2"}, &session, &error));
    CHECK(!session.empty());
    CHECK_EQ(patientSide.currentSession(), session);
    CHECK(fs::is_directory(shared / "hospitals" / "H1" / "from_admin"));
    CHECK(fs::is_directory(shared / "hospitals" / "H2" / "to_admin"));

    Request a;
    a.id = "R1000-001";
    a.injuryCode = "STROKE";
    a.answers = {1};
    Request b = a;
    b.id = "R0999-001";  // older id, should be collected first

    CHECK(patientSide.sendRequest(a.id, requestToMessage(a), &error));
    CHECK(patientSide.sendRequest(b.id, requestToMessage(b), &error));

    // A half-written file must be ignored.
    std::ofstream(shared / "requests" / "req_R0001-000.txt.tmp") << "id=partial";

    std::vector<std::string> problems;
    std::vector<Message> received = adminSide.collectRequests(&problems);
    CHECK_EQ(received.size(), 2u);
    CHECK(problems.empty());
    if (received.size() == 2) {
        CHECK_EQ(received[0].get("id"), std::string("R0999-001"));
        CHECK_EQ(received[1].get("id"), std::string("R1000-001"));
    }
    CHECK(adminSide.collectRequests(&problems).empty());  // files were removed

    // Corrupt request is reported and removed.
    std::ofstream(shared / "requests" / "req_R2000-000.txt") << "garbage without equals";
    CHECK(adminSide.collectRequests(&problems).empty());
    CHECK_EQ(problems.size(), 1u);

    // Admin replies; patient reads updates in order, only once each.
    int nextSeq = 0;
    CHECK(patientSide.readUpdates(a.id, &nextSeq).empty());
    for (int seq = 0; seq <= 1; ++seq) {
        Message update;
        update.set("type", seq == 0 ? "RECEIVED" : "DISPATCHED");
        CHECK(adminSide.sendUpdate(a.id, seq, update, &error));
    }
    std::vector<Message> updates = patientSide.readUpdates(a.id, &nextSeq);
    CHECK_EQ(updates.size(), 2u);
    CHECK_EQ(nextSeq, 2);
    if (updates.size() == 2) CHECK_EQ(updates[1].get("type"), std::string("DISPATCHED"));
    CHECK(patientSide.readUpdates(a.id, &nextSeq).empty());

    // The admin's clock is visible to every window.
    double minutes = 0, speed = 0;
    CHECK(!patientSide.readClock(&minutes, &speed));
    CHECK(adminSide.writeClock(605.5, 60, &error));
    CHECK(patientSide.readClock(&minutes, &speed));
    CHECK_NEAR(minutes, 605.5, 1e-6);
    CHECK_NEAR(speed, 60.0, 1e-6);

    // A new admin session clears everything.
    std::string second;
    CHECK(adminSide.startSession({"H1"}, &second, &error));
    CHECK(second != session);
    nextSeq = 0;
    CHECK(patientSide.readUpdates(a.id, &nextSeq).empty());

    fs::remove_all(shared);
}

static void testHospitalChannel() {
    testkit::section("Channel: admin <-> hospital");
    fs::path shared = fs::temp_directory_path() / "rahat_comms_test_h";
    fs::remove_all(shared);

    Channel admin(shared.string());
    Channel h1(shared.string());
    std::string error;
    std::string session;
    CHECK(admin.startSession({"H1", "H2"}, &session, &error));

    // Admin -> hospital: numbered, read in order, only by that hospital.
    int nextSeq = 0;
    for (int seq = 0; seq <= 2; ++seq) {
        Message m;
        m.set("kind", seq == 0 ? "STATUS" : "ASSIGNMENT");
        CHECK(admin.sendToHospital("H1", seq, m, &error));
    }
    std::vector<Message> inbox = h1.readFromAdmin("H1", &nextSeq);
    CHECK_EQ(inbox.size(), 3u);
    CHECK_EQ(nextSeq, 3);
    int otherSeq = 0;
    CHECK(h1.readFromAdmin("H2", &otherSeq).empty());

    // Hospital -> admin: collected once, oldest first.
    Message first;
    first.set("kind", "RETURNED");
    first.set("ambulance", "A1");
    Message second;
    second.set("kind", "RETURNED");
    second.set("ambulance", "A2");
    CHECK(h1.sendToAdmin("H1", first, &error));
    CHECK(h1.sendToAdmin("H1", second, &error));
    std::vector<std::string> problems;
    std::vector<Message> pressed = admin.collectFromHospital("H1", &problems);
    CHECK_EQ(pressed.size(), 2u);
    if (pressed.size() == 2) {
        CHECK_EQ(pressed[0].get("ambulance"), std::string("A1"));
        CHECK_EQ(pressed[1].get("ambulance"), std::string("A2"));
    }
    CHECK(admin.collectFromHospital("H1", &problems).empty());
    CHECK(admin.collectFromHospital("H404", &problems).empty());  // unknown hospital: nothing, no crash

    fs::remove_all(shared);
}

int main() {
    testMessage();
    testRequestConversion();
    testChannel();
    testHospitalChannel();
    return testkit::summary("comms");
}
