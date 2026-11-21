// Hospital module: hospitals, their doctors, and choosing one for a patient.
//
//  rankFor():  hospitals that have the needed facilities and a specialist on
//              duty right now, nearest (by traffic) first.
//  checkAll(): every hospital, nearest first, with whether it can take the
//              patient right now and, if not, the reason. Phase 3 has no
//              accept / reject step: the system assigns the first suitable one.
//
// A doctor carries `maxPatients` load units at once. Each patient uses units
// by severity (MINOR/MODERATE 1, SERIOUS 2, CRITICAL the whole doctor). The
// units of all the hospital's doctors for one specialty form one pool: a
// patient goes to any on-duty doctor with enough units free. Units are
// reserved when a patient is assigned and released after treatment.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ds/hash_map.h"
#include "../ds/priority_queue.h"
#include "../models/models.h"

namespace rahat {

struct HospitalCandidate {
    Hospital* hospital = nullptr;
    double minutesToPatient = 0;  // one way, with current traffic
};

// Result of checking one hospital for one request.
struct HospitalCheck {
    const Hospital* hospital = nullptr;
    double minutesToPatient = 0;  // one way, with current traffic; infinity if unreachable
    int capacity = 0;             // patients it can take right now (0 = not suitable)
    std::string reason;           // why it cannot take the patient; empty when capacity > 0

    bool suitable() const { return capacity > 0; }
};

class HospitalManager {
public:
    HospitalManager();
    ~HospitalManager();
    HospitalManager(const HospitalManager&) = delete;
    HospitalManager& operator=(const HospitalManager&) = delete;

    Hospital* addHospital(const std::string& id, const std::string& name, int landmarkId,
                          const std::vector<std::string>& facilities, std::string* error);
    Doctor* addDoctor(const Doctor& doctor, std::string* error);  // hospital must exist
    bool linkAmbulance(Ambulance* ambulance, std::string* error);  // ambulance's home hospital

    Hospital* find(const std::string& id);
    const Hospital* find(const std::string& id) const;
    const std::vector<Hospital*>& all() const { return hospitals; }

    // ---- RAHAT side
    // minutesFromPatient(landmarkId) returns travel minutes (infinity if unreachable).
    std::vector<HospitalCandidate> rankFor(const Request& request, int minuteOfDay,
                                           const std::function<double(int)>& minutesFromPatient) const;

    // Every hospital, reachable ones nearest first, unreachable ones last.
    // freeAmbulances(hospitalId) returns the free ambulances suitable for this request.
    // Reasons are checked in this order: not reachable, missing facility,
    // no specialist on duty, specialist busy, no free ambulance.
    std::vector<HospitalCheck> checkAll(const Request& request, int minuteOfDay,
                                        const std::function<double(int)>& minutesFromPatient,
                                        const std::function<int(const std::string&)>& freeAmbulances) const;

    // How many patients of this severity the on-duty specialists can take now.
    int patientsFittable(const Hospital& hospital, const std::string& specialization, int minuteOfDay,
                         Severity severity) const;
    int onDutyCount(const Hospital& hospital, const std::string& specialization, int minuteOfDay) const;

    // Reserves the load for one patient at the least loaded on-duty specialist that has room.
    // Sets *units to the load taken. nullptr if no specialist has room.
    Doctor* reserveDoctor(Hospital& hospital, const std::string& specialization, int minuteOfDay, Severity severity,
                          int* units);
    void scheduleRelease(Doctor* doctor, double atMinute, int units);
    int releaseDue(double now);  // frees load whose treatment time is over

private:
    struct Release {
        Doctor* doctor;
        double at;
        int units;
    };
    struct EarliestReleaseFirst {
        bool operator()(const Release& a, const Release& b) const { return a.at < b.at; }
    };

    HashMap<std::string, Hospital*> byId;
    HashMap<std::string, Doctor*> doctorsById;
    std::vector<Hospital*> hospitals;
    std::vector<Doctor*> doctors;
    PriorityQueue<Release, EarliestReleaseFirst> releases;
};

}  // namespace rahat
