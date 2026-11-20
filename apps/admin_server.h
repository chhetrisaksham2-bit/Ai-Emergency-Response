// AdminServer: everything the admin / server window does, without the screen.
//
// One step():
//   1. collect new requests from patient windows and the generator -> engine.submit()
//   2. collect button presses from every hospital window -> engine.confirmReturn()
//   3. engine.tick()
//   4. send each engine event where it belongs:
//        patient window   -> its own updates
//        hospital window  -> assignments and trip progress for that hospital only
//   5. send each hospital its ambulance status when it changes, and write the clock
//
// The admin window calls step() from a background thread and draws view().
// Tests call step() directly with chosen times.
#pragma once

#include <string>
#include <vector>

#include "../modules/comms/comms.h"
#include "../modules/data_io/data_io.h"
#include "../modules/dispatch/dispatch.h"

namespace rahat {

struct AdminView {
    struct RequestRow {
        std::string id;
        std::string injury;
        std::string landmark;
        std::string severity;
        int priorityScore = 0;
        int patients = 1;
        std::string status;
        std::string hospitals;            // where its patients are going
        std::vector<std::string> checks;  // every hospital and why it was / was not selected
    };
    struct HospitalRow {
        std::string id;
        std::string name;
        int total = 0;
        int freeAls = 0;
        int freeBls = 0;
    };
    struct AmbulanceRow {
        std::string id;
        std::string hospital;
        std::string type;
        std::string status;
        std::string request;
    };

    std::string clock;
    double speed = 1;
    std::string batch;  // "collecting new requests until HH:MM" or empty
    std::vector<RequestRow> active;  // most urgent first
    int completed = 0;
    std::vector<HospitalRow> hospitals;
    std::vector<AmbulanceRow> ambulances;
    std::vector<std::string> log;  // oldest first
};

class AdminServer {
public:
    AdminServer(World& world, const std::string& sharedDir, EngineSettings settings);

    // Starts a new session: clears the shared folder and creates the hospital folders.
    bool start(std::string* error);

    // Does one round of work at simulated time `now`. Returns true if anything changed.
    bool step(double now, double speed = 1);

    AdminView view(double now) const;
    const std::string& session() const { return sessionId; }
    DispatchEngine& dispatch() { return engine; }

private:
    World& world;
    Channel channel;
    DispatchEngine engine;
    std::string sessionId;

    HashMap<std::string, int> patientSeq;   // next update number per request
    HashMap<std::string, int> hospitalSeq;  // next message number per hospital
    HashMap<std::string, std::string> lastStatus;
    std::vector<std::string> log;

    void addLog(double time, const std::string& line);
    void toPatient(const std::string& requestId, double time, const std::string& type, const std::string& text,
                   const std::vector<std::string>& details, const std::string& destination = "",
                   const std::string& severity = "");
    void toHospital(const std::string& hospitalId, const Message& message);
    void routeEvent(const EngineEvent& event);
    bool sendStatuses();
    std::string statusText(const std::string& hospitalId) const;
};

}  // namespace rahat
