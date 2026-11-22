// Dispatch module: the RAHAT engine that links every other module together.
//
//  submit()  new request -> triage -> priority queue
//  tick()    1. free doctors whose treatment time is over
//            2. move ambulances along their routes; count back ambulances whose
//               return was never confirmed (return timeout)
//            3. retry waiting requests
//            4. when the batch window is over (or a CRITICAL request is waiting),
//               serve the queue, most urgent first:
//                 check every hospital (reachable, facilities, specialist on duty,
//                 specialist free, free ambulance), nearest by traffic first
//                 assign the patient to the first suitable one - no accept / reject
//                 dispatch an ambulance from that hospital per patient
//  confirmReturn()  the hospital's "ambulance is back" button
//
// Case 1: one request.  Case 2: many requests -> the queue decides who goes first.
// Case 3: many patients at one place -> spread across ambulances and hospitals.
//
// The engine never touches files or the screen. It produces EngineEvents; the
// admin window shows them and sends them to the patient and hospital windows.
#pragma once

#include <string>
#include <vector>

#include "../ambulance/ambulance.h"
#include "../firstaid/firstaid.h"
#include "../hospital/hospital.h"
#include "../routing/routing.h"
#include "../triage/triage.h"

namespace rahat {

struct EngineSettings {
    double sceneMinutes = 5.0;           // time at the scene before leaving with the patient
    double retryMinutes = 2.0;           // when no hospital could take the patient
    double treatmentMinutes = 60.0;      // a doctor stays busy this long after the patient arrives
    double returnTimeoutMinutes = 75.0;  // ambulance counted back this long after dispatch if not confirmed
    double batchMinutes = 1.0;           // new requests are collected this long, then served together
};

enum class EventType {
    Received,
    HospitalsChecked,  // every hospital with why it was or was not selected
    Dispatched,
    FirstAid,
    ArrivedAtScene,
    LeftScene,
    ArrivedAtHospital,
    Completed,
    Waiting,
    AmbulanceReturned
};
std::string toString(EventType type);

struct EngineEvent {
    double time = 0;
    std::string requestId;
    EventType type = EventType::Received;
    std::string text;                  // one line for people
    std::vector<std::string> details;  // extra lines: hospital list, route, first-aid steps
    bool forPatient = true;            // false = only for the hospital / admin side
    std::string hospitalId;            // set when the event concerns one hospital
    std::string ambulanceId;           // set when the event concerns one ambulance
};

struct Assignment {
    std::string ambulanceId;
    std::string hospitalId;
    std::string doctorName;
    double dispatchedAt = 0;
    double arriveSceneAt = 0;
    double arriveHospitalAt = 0;
};

class DispatchEngine {
public:
    DispatchEngine(CityMap& map, const InjuryCatalog& injuries, const FirstAidGuide& firstAid,
                   HospitalManager& hospitals, AmbulanceManager& ambulances, EngineSettings settings = EngineSettings());
    ~DispatchEngine();
    DispatchEngine(const DispatchEngine&) = delete;
    DispatchEngine& operator=(const DispatchEngine&) = delete;

    // Takes ownership of `request` when it returns true.
    bool submit(Request* request, double now, std::string* error);

    void tick(double now);

    // The hospital pressed "ambulance is back". Returns false with a reason if not allowed.
    bool confirmReturn(const std::string& hospitalId, const std::string& ambulanceId, double now,
                       std::string* error);

    // Events produced since the last call, oldest first.
    std::vector<EngineEvent> takeEvents();

    // ---- Views for the windows
    std::vector<const Request*> activeRequests() const;  // not completed, most urgent first
    int completedCount() const;
    const Request* findRequest(const std::string& id) const;
    std::vector<Assignment> assignmentsFor(const std::string& requestId) const;
    std::vector<std::string> hospitalCheckLines(const std::string& requestId) const;  // last check
    double nextActionAt(const std::string& requestId) const;                          // retry time
    bool batchWaiting(double now, double* servesAt) const;  // true while new requests are being collected
    const EngineSettings& settings() const { return config; }
    std::string injuryName(const Request& request) const;
    std::string hospitalName(const std::string& hospitalId) const;

private:
    struct Case {
        Request* request = nullptr;
        PathTree tree;  // Dijkstra from the patient's landmark
        double waitUntil = 0;
        std::vector<Assignment> assignments;
        std::vector<std::string> checkLines;   // last "hospitals checked" list
        HashMap<std::string, bool> announced;  // messages already sent, so retries do not repeat them
    };

    CityMap& map;
    const InjuryCatalog& injuries;
    const FirstAidGuide& firstAid;
    HospitalManager& hospitals;
    AmbulanceManager& ambulances;
    EngineSettings config;
    Triage triage;

    HashMap<std::string, Case*> cases;
    std::vector<Case*> caseOrder;
    RequestQueue queue;
    long nextSequence = 1;
    std::vector<EngineEvent> pending;
    double batchOpenedAt = -1;   // first new request of the current batch; -1 = no batch open
    bool criticalWaiting = false;

    EngineEvent& emit(double time, const Request& request, EventType type, const std::string& text,
                      const std::vector<std::string>& details = {}, bool forPatient = true);
    // Like emit, but skips a message this case has already announced (used while retrying).
    void emitOnce(Case& c, double time, EventType type, const std::string& text,
                  const std::vector<std::string>& details = {});
    void allocate(Case& c, double now);
    int dispatchPatients(Case& c, Hospital& hospital, int count, double now);
    void onTripEvent(const TripEvent& event);
    void wakeWaitingCases(double now);
};

}  // namespace rahat
