// HospitalClient: everything one hospital window does, without the screen.
//
// It only reads its own folder (hospitals/<id>/from_admin) and only writes
// button presses to hospitals/<id>/to_admin. It never sees other hospitals'
// requests or the admin's queue.
#pragma once

#include <string>
#include <vector>

#include "../modules/comms/comms.h"

namespace rahat {

struct HospitalAssignment {
    std::string requestId;
    std::string ambulanceId;
    std::string ambulanceType;
    std::string injury;
    std::string severity;
    int points = 0;
    int patients = 1;
    std::string pickup;       // the patient's landmark: where the ambulance picks the patient up
    std::string destination;  // this hospital
    std::string dispatchedAt;
    std::string etaScene;
    std::string etaHospital;
    std::string doctor;
    std::vector<std::string> route;
    std::vector<std::string> firstAid;
    std::vector<std::string> timeline;  // "HH:MM text", oldest first
    std::string stage = "DISPATCHED";   // last progress type
    bool returned = false;              // ambulance available again
};

struct HospitalAmbulance {
    std::string id;
    std::string type;
    std::string status;
    std::string requestId;
};

class HospitalClient {
public:
    HospitalClient(const std::string& sharedDir, const std::string& hospitalId);

    // Reads what the admin sent since the last call. Returns true if anything changed.
    // If the admin started a new session, everything is cleared and read again.
    bool poll();

    // The "ambulance is back" button. The admin answers with a RESULT message.
    bool pressReturned(const std::string& ambulanceId, std::string* error);

    bool connected() const { return !session.empty(); }
    const std::string& id() const { return hospitalId; }
    const std::string& name() const { return hospitalName; }
    const std::vector<HospitalAssignment>& assignments() const { return list; }  // newest first
    const std::vector<HospitalAmbulance>& ambulances() const { return fleet; }
    int freeAmbulances() const;
    const std::string& lastResult() const { return result; }
    bool lastResultOk() const { return resultOk; }
    std::string clock() const;  // simulated time from the admin, or "--:--"

private:
    Channel channel;
    std::string hospitalId;
    std::string hospitalName;
    std::string session;
    int nextSeq = 0;
    std::vector<HospitalAssignment> list;
    std::vector<HospitalAmbulance> fleet;
    std::string result;
    bool resultOk = true;
    std::string lastClock;

    void reset();
    void apply(const Message& message);
    HospitalAssignment* find(const std::string& requestId, const std::string& ambulanceId);
};

}  // namespace rahat
