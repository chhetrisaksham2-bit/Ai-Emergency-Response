#include "hospital.h"

#include <cmath>

namespace rahat {

namespace {

struct NearestFirst {
    bool operator()(const HospitalCandidate& a, const HospitalCandidate& b) const {
        if (a.minutesToPatient != b.minutesToPatient) return a.minutesToPatient < b.minutesToPatient;
        return a.hospital->id < b.hospital->id;  // stable order for equal times
    }
};

}  // namespace

HospitalManager::HospitalManager() = default;

HospitalManager::~HospitalManager() {
    for (Doctor* doctor : doctors) delete doctor;
    for (Hospital* hospital : hospitals) delete hospital;
}

Hospital* HospitalManager::addHospital(const std::string& id, const std::string& name, int landmarkId,
                                       const std::vector<std::string>& facilities, std::string* error) {
    if (byId.contains(id)) {
        *error = "Duplicate hospital id " + id;
        return nullptr;
    }
    Hospital* hospital = new Hospital();
    hospital->id = id;
    hospital->name = name;
    hospital->landmarkId = landmarkId;
    hospital->facilities = facilities;
    byId.put(id, hospital);
    hospitals.push_back(hospital);
    return hospital;
}

Doctor* HospitalManager::addDoctor(const Doctor& doctor, std::string* error) {
    Hospital* hospital = find(doctor.hospitalId);
    if (hospital == nullptr) {
        *error = "Doctor " + doctor.id + " belongs to unknown hospital " + doctor.hospitalId;
        return nullptr;
    }
    if (doctorsById.contains(doctor.id)) {
        *error = "Duplicate doctor id " + doctor.id;
        return nullptr;
    }
    if (doctor.maxPatients < 1) {
        *error = "Doctor " + doctor.id + " needs max_patients of at least 1";
        return nullptr;
    }
    Doctor* stored = new Doctor(doctor);
    stored->activeLoad = 0;
    doctorsById.put(stored->id, stored);
    doctors.push_back(stored);
    hospital->doctors.push_back(stored);
    return stored;
}

bool HospitalManager::linkAmbulance(Ambulance* ambulance, std::string* error) {
    Hospital* hospital = find(ambulance->hospitalId);
    if (hospital == nullptr) {
        *error = "Ambulance " + ambulance->id + " belongs to unknown hospital " + ambulance->hospitalId;
        return false;
    }
    hospital->ambulances.push_back(ambulance);
    return true;
}

Hospital* HospitalManager::find(const std::string& id) {
    Hospital** found = byId.find(id);
    return found == nullptr ? nullptr : *found;
}

const Hospital* HospitalManager::find(const std::string& id) const {
    Hospital* const* found = byId.find(id);
    return found == nullptr ? nullptr : *found;
}

std::vector<HospitalCandidate> HospitalManager::rankFor(const Request& request, int minuteOfDay,
                                                        const std::function<double(int)>& minutesFromPatient) const {
    PriorityQueue<HospitalCandidate, NearestFirst> nearest;
    for (Hospital* hospital : hospitals) {
        if (!hospital->hasAllFacilities(request.requiredFacilities)) continue;
        if (onDutyCount(*hospital, request.requiredSpecialization, minuteOfDay) == 0) continue;
        double minutes = minutesFromPatient(hospital->landmarkId);
        if (std::isinf(minutes)) continue;  // no road connects them
        nearest.push({hospital, minutes});
    }

    std::vector<HospitalCandidate> ranked;
    while (!nearest.empty()) ranked.push_back(nearest.pop());
    return ranked;
}

namespace {

std::string facilityList(const std::vector<std::string>& facilities) {
    std::string text;
    for (const std::string& f : facilities) text += (text.empty() ? "" : ", ") + humanize(f);
    return text;
}

struct CheckOrder {  // reachable nearest first, then unreachable; ties by id
    bool operator()(const HospitalCheck& a, const HospitalCheck& b) const {
        bool aFar = std::isinf(a.minutesToPatient), bFar = std::isinf(b.minutesToPatient);
        if (aFar != bFar) return !aFar;
        if (!aFar && a.minutesToPatient != b.minutesToPatient) return a.minutesToPatient < b.minutesToPatient;
        return a.hospital->id < b.hospital->id;
    }
};

}  // namespace

std::vector<HospitalCheck> HospitalManager::checkAll(
    const Request& request, int minuteOfDay, const std::function<double(int)>& minutesFromPatient,
    const std::function<int(const std::string&)>& freeAmbulances) const {
    PriorityQueue<HospitalCheck, CheckOrder> ordered;
    for (Hospital* hospital : hospitals) {
        HospitalCheck check;
        check.hospital = hospital;
        check.minutesToPatient = minutesFromPatient(hospital->landmarkId);

        std::vector<std::string> missing;
        for (const std::string& f : request.requiredFacilities) {
            if (!hospital->hasFacility(f)) missing.push_back(f);
        }
        int slots = patientsFittable(*hospital, request.requiredSpecialization, minuteOfDay, request.severity);
        int ambulancesFree = freeAmbulances(hospital->id);

        if (std::isinf(check.minutesToPatient)) {
            check.reason = "not reachable by road";
        } else if (!missing.empty()) {
            check.reason = "no " + facilityList(missing);
        } else if (onDutyCount(*hospital, request.requiredSpecialization, minuteOfDay) == 0) {
            check.reason = "no " + humanize(request.requiredSpecialization) + " on duty";
        } else if (slots <= 0) {
            check.reason = "every " + humanize(request.requiredSpecialization) + " is busy";
        } else if (ambulancesFree <= 0) {
            check.reason = "no " + toString(request.requiredAmbulance) + " ambulance available right now";
        } else {
            check.capacity = slots < ambulancesFree ? slots : ambulancesFree;
        }
        ordered.push(check);
    }

    std::vector<HospitalCheck> result;
    while (!ordered.empty()) result.push_back(ordered.pop());
    return result;
}

namespace {

// Load units one patient of this severity takes from this doctor.
// CRITICAL takes the whole doctor, so nobody else is treated at the same time.
int unitsFor(const Doctor& doctor, Severity severity) {
    if (severity == Severity::Critical) return doctor.maxPatients;
    return severity == Severity::Serious ? 2 : 1;
}

bool hasRoomFor(const Doctor& doctor, Severity severity) {
    if (severity == Severity::Critical) return doctor.activeLoad == 0;
    return doctor.freeLoad() >= unitsFor(doctor, severity);
}

}  // namespace

int HospitalManager::patientsFittable(const Hospital& hospital, const std::string& specialization, int minuteOfDay,
                                      Severity severity) const {
    int count = 0;
    for (const Doctor* doctor : hospital.doctors) {
        if (doctor->specialization != specialization || !doctor->onDuty(minuteOfDay)) continue;
        if (severity == Severity::Critical) {
            if (hasRoomFor(*doctor, severity)) ++count;
        } else {
            count += doctor->freeLoad() / unitsFor(*doctor, severity);
        }
    }
    return count;
}

int HospitalManager::onDutyCount(const Hospital& hospital, const std::string& specialization,
                                 int minuteOfDay) const {
    int count = 0;
    for (const Doctor* doctor : hospital.doctors) {
        if (doctor->specialization == specialization && doctor->onDuty(minuteOfDay)) ++count;
    }
    return count;
}

Doctor* HospitalManager::reserveDoctor(Hospital& hospital, const std::string& specialization, int minuteOfDay,
                                       Severity severity, int* units) {
    Doctor* chosen = nullptr;
    for (Doctor* doctor : hospital.doctors) {
        if (doctor->specialization != specialization || !doctor->onDuty(minuteOfDay)) continue;
        if (!hasRoomFor(*doctor, severity)) continue;
        if (chosen == nullptr || doctor->activeLoad < chosen->activeLoad) chosen = doctor;
    }
    *units = 0;
    if (chosen != nullptr) {
        *units = unitsFor(*chosen, severity);
        chosen->activeLoad += *units;
    }
    return chosen;
}

void HospitalManager::scheduleRelease(Doctor* doctor, double atMinute, int units) {
    releases.push({doctor, atMinute, units});
}

int HospitalManager::releaseDue(double now) {
    int released = 0;
    while (!releases.empty() && releases.top().at <= now) {
        Release release = releases.pop();
        release.doctor->activeLoad -= release.units;
        if (release.doctor->activeLoad < 0) release.doctor->activeLoad = 0;
        ++released;
    }
    return released;
}

}  // namespace rahat
