// Core models shared by every RAHAT module: the entities (Request, Hospital,
// Doctor, Ambulance), their enums, and time helpers.
//
// Ownership: managers create these objects with `new` and delete them.
// A Hospital only *points* to its doctors and ambulances; it does not own them.
#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace rahat {

// ---------------------------------------------------------------- enums

enum class Severity { Minor = 1, Moderate = 2, Serious = 3, Critical = 4 };

// BLS = Basic Life Support, ALS = Advanced Life Support (defibrillator, ventilator, drugs).
enum class AmbulanceType { BLS, ALS };

// AtHospital: the patient has been handed over, but the hospital has not yet
// confirmed that the ambulance is ready again (return button or timeout).
enum class AmbulanceStatus { Available, ToScene, AtScene, ToHospital, AtHospital };

enum class RequestStatus {
    Queued,            // waiting in the priority queue
    Waiting,           // no hospital could take it; will retry
    InProgress,        // every patient has an ambulance on the way
    Completed          // every patient reached a hospital
};

std::string toString(Severity severity);
std::string toString(AmbulanceType type);
std::string toString(AmbulanceStatus status);
std::string toString(RequestStatus status);

bool parseSeverity(const std::string& text, Severity* out);
bool parseAmbulanceType(const std::string& text, AmbulanceType* out);

// "GENERAL_SURGEON" -> "general surgeon" for messages shown to people.
std::string humanize(const std::string& code);

// ---------------------------------------------------------------- time
// Simulated time is a double counting minutes. minuteOfDay() wraps it to 0..1439.

bool parseClock(const std::string& text, int* minuteOfDay);  // "HH:MM" -> minutes
std::string formatClock(double minutes);                      // minutes -> "HH:MM"
int minuteOfDay(double minutes);
// True when minuteOfDay lies in [start, end). Handles overnight shifts like 20:00-08:00.
bool inTimeWindow(int minuteOfDay, int start, int end);

// A clock that runs `speed` times faster than real time, starting at a chosen time of day.
class SimClock {
public:
    SimClock(int startMinuteOfDay, double speed);
    double now() const;  // simulated minutes since midnight of day 0
    double speed() const { return speedFactor; }

private:
    std::chrono::steady_clock::time_point realStart;
    double startMinutes;
    double speedFactor;
};

int currentSystemMinuteOfDay();

// ---------------------------------------------------------------- entities

// One stop on an ambulance route.
struct Checkpoint {
    int landmarkId;
    std::string name;
    double atMinute;  // when the ambulance reaches it (relative or absolute, set by the builder)
};

struct Doctor {
    std::string id;
    std::string hospitalId;
    std::string name;
    std::string specialization;
    int shiftStart = 0;  // minute of day
    int shiftEnd = 0;
    // Load units this doctor can carry at once (max_patients in doctors.csv).
    // A MINOR or MODERATE patient uses 1 unit, a SERIOUS patient 2, and a
    // CRITICAL patient needs the whole doctor (see hospital.cpp).
    int maxPatients = 1;
    int activeLoad = 0;  // units in use right now

    bool onDuty(int minuteOfDay) const { return inTimeWindow(minuteOfDay, shiftStart, shiftEnd); }
    int freeLoad() const { return maxPatients - activeLoad; }
};

struct Ambulance {
    std::string id;
    std::string hospitalId;  // home hospital
    AmbulanceType type = AmbulanceType::BLS;
    AmbulanceStatus status = AmbulanceStatus::Available;
    std::string requestId;   // current job, empty when available
    double dispatchedAt = 0;  // when the current job started
    double releaseAt = 0;     // AtHospital only: counted as available from this time if not confirmed
};

struct Hospital {
    std::string id;
    std::string name;
    int landmarkId = 0;
    std::vector<std::string> facilities;  // e.g. ICU, CT_SCAN, ANTIVENOM
    std::vector<Doctor*> doctors;         // not owned
    std::vector<Ambulance*> ambulances;   // not owned

    bool hasFacility(const std::string& facility) const;
    bool hasAllFacilities(const std::vector<std::string>& required) const;
};

struct Request {
    // Filled from the patient's window
    std::string id;
    std::string injuryCode;    // e.g. BLEEDING
    std::vector<int> answers;  // chosen option (0-based) for each intensity question
    int landmarkId = 0;
    int patientCount = 1;

    // Filled by the dispatch engine
    double receivedAt = 0;  // simulated minute
    long sequence = 0;      // arrival order, breaks ties

    // Filled by triage
    int points = 0;         // from intensity answers
    int priorityScore = 0;  // base priority + points
    Severity severity = Severity::Minor;
    std::string requiredSpecialization;
    std::vector<std::string> requiredFacilities;
    AmbulanceType requiredAmbulance = AmbulanceType::BLS;

    // Progress
    RequestStatus status = RequestStatus::Queued;
    int patientsAssigned = 0;
    int patientsDelivered = 0;
};

}  // namespace rahat
