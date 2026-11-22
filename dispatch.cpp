#include "dispatch.h"

#include <cmath>

namespace rahat {

namespace {

std::string minutesText(double minutes) {
    if (minutes >= 60 && std::fmod(minutes, 60.0) < 1e-9) {
        long hours = static_cast<long>(minutes / 60);
        return std::to_string(hours) + (hours == 1 ? " hour" : " hours");
    }
    if (minutes > 60) {
        long whole = static_cast<long>(std::ceil(minutes - 1e-9));
        return std::to_string(whole / 60) + " h " + std::to_string(whole % 60) + " min";
    }
    long whole = static_cast<long>(std::ceil(minutes - 1e-9));
    return std::to_string(whole < 0 ? 0 : whole) + " min";
}

std::string patientsText(int count) {
    return std::to_string(count) + (count == 1 ? " patient" : " patients");
}

}  // namespace

std::string toString(EventType type) {
    switch (type) {
        case EventType::Received: return "RECEIVED";
        case EventType::HospitalsChecked: return "HOSPITALS_CHECKED";
        case EventType::Dispatched: return "DISPATCHED";
        case EventType::FirstAid: return "FIRST_AID";
        case EventType::ArrivedAtScene: return "ARRIVED_AT_SCENE";
        case EventType::LeftScene: return "LEFT_SCENE";
        case EventType::ArrivedAtHospital: return "ARRIVED_AT_HOSPITAL";
        case EventType::Completed: return "COMPLETED";
        case EventType::Waiting: return "WAITING";
        case EventType::AmbulanceReturned: return "AMBULANCE_RETURNED";
    }
    return "UNKNOWN";
}

DispatchEngine::DispatchEngine(CityMap& map, const InjuryCatalog& injuries, const FirstAidGuide& firstAid,
                               HospitalManager& hospitals, AmbulanceManager& ambulances, EngineSettings settings)
    : map(map),
      injuries(injuries),
      firstAid(firstAid),
      hospitals(hospitals),
      ambulances(ambulances),
      config(settings),
      triage(injuries) {
    ambulances.setReturnTimeout(config.returnTimeoutMinutes);
}

DispatchEngine::~DispatchEngine() {
    for (Case* c : caseOrder) {
        delete c->request;
        delete c;
    }
}

// ------------------------------------------------------------ submit

bool DispatchEngine::submit(Request* request, double now, std::string* error) {
    if (request == nullptr || request->id.empty()) {
        *error = "Request has no id";
        return false;
    }
    if (cases.contains(request->id)) {
        *error = "Request " + request->id + " was already received";
        return false;
    }
    if (!map.hasLandmark(request->landmarkId)) {
        *error = "Unknown landmark " + std::to_string(request->landmarkId);
        return false;
    }
    if (!triage.assess(*request, error)) return false;

    request->receivedAt = now;
    request->sequence = nextSequence++;
    request->status = RequestStatus::Queued;
    request->patientsAssigned = 0;
    request->patientsDelivered = 0;

    Case* c = new Case();
    c->request = request;
    cases.put(request->id, c);
    caseOrder.push_back(c);
    queue.push(request);
    if (batchOpenedAt < 0) batchOpenedAt = now;
    if (request->severity == Severity::Critical) criticalWaiting = true;

    std::string text = injuryName(*request) + " at " + map.landmarkName(request->landmarkId) + " - " +
                       toString(request->severity) + ", priority " + std::to_string(request->priorityScore);
    if (request->patientCount > 1) text += ", " + patientsText(request->patientCount);
    emit(now, *request, EventType::Received, "Request received: " + text + ".");
    return true;
}

// ------------------------------------------------------------ tick

void DispatchEngine::tick(double now) {
    bool resourcesFreed = hospitals.releaseDue(now) > 0;

    for (const TripEvent& event : ambulances.update(now)) {
        onTripEvent(event);
        if (event.type == TripEventType::ReturnTimedOut) resourcesFreed = true;
    }
    if (resourcesFreed) wakeWaitingCases(now);

    for (Case* c : caseOrder) {
        Request& r = *c->request;
        if (r.status == RequestStatus::Waiting && now >= c->waitUntil) {
            r.status = RequestStatus::Queued;
            queue.push(&r);
        }
    }

    // New requests are collected for the batch window, then served together,
    // so the most urgent one is served first even if it arrived a little later.
    // A CRITICAL request does not wait for the window.
    if (batchOpenedAt >= 0 && !criticalWaiting && now < batchOpenedAt + config.batchMinutes) return;
    batchOpenedAt = -1;
    criticalWaiting = false;

    // Most urgent first. allocate() always moves a request out of the Queued state.
    while (!queue.empty()) {
        Request* request = queue.pop();
        Case** c = cases.find(request->id);
        if (c != nullptr && request->status == RequestStatus::Queued) allocate(**c, now);
    }
}

bool DispatchEngine::batchWaiting(double now, double* servesAt) const {
    if (batchOpenedAt < 0 || criticalWaiting) return false;
    *servesAt = batchOpenedAt + config.batchMinutes;
    return now < *servesAt;
}

void DispatchEngine::wakeWaitingCases(double now) {
    for (Case* c : caseOrder) {
        if (c->request->status == RequestStatus::Waiting && c->waitUntil > now) c->waitUntil = now;
    }
}

bool DispatchEngine::confirmReturn(const std::string& hospitalId, const std::string& ambulanceId, double now,
                                   std::string* error) {
    std::vector<TripEvent> events;
    if (!ambulances.confirmReturn(ambulanceId, hospitalId, now, &events, error)) return false;
    for (const TripEvent& event : events) onTripEvent(event);
    wakeWaitingCases(now);
    return true;
}

// ------------------------------------------------------------ allocation

void DispatchEngine::allocate(Case& c, double now) {
    Request& r = *c.request;
    int minute = minuteOfDay(now);

    // Checked again every time: ambulances, doctors and traffic change.
    c.tree = map.shortestFrom(r.landmarkId, minute);
    std::vector<HospitalCheck> checks = hospitals.checkAll(
        r, minute, [&](int landmark) { return map.minutesTo(c.tree, landmark); },
        [&](const std::string& hospitalId) { return ambulances.countFreeSuitable(hospitalId, r.requiredAmbulance); });

    // Plan: nearest suitable hospitals first, each takes as many patients as it can.
    int remaining = r.patientCount - r.patientsAssigned;
    std::vector<int> planned(checks.size(), 0);
    for (size_t i = 0; i < checks.size() && remaining > 0; ++i) {
        if (!checks[i].suitable()) continue;
        planned[i] = checks[i].capacity < remaining ? checks[i].capacity : remaining;
        remaining -= planned[i];
    }

    std::vector<std::string> lines;
    for (size_t i = 0; i < checks.size(); ++i) {
        const HospitalCheck& check = checks[i];
        std::string away = std::isinf(check.minutesToPatient) ? "no road" : minutesText(check.minutesToPatient) + " away";
        std::string verdict;
        if (planned[i] > 0) {
            verdict = "SELECTED" + (r.patientCount > 1 ? " for " + patientsText(planned[i]) : std::string());
        } else if (check.suitable()) {
            verdict = "not selected: a nearer hospital was selected";
        } else {
            verdict = "not selected: " + check.reason;
        }
        lines.push_back(std::to_string(i + 1) + ". " + check.hospital->name + " (" + away + ") - " + verdict);
    }
    c.checkLines = lines;

    bool anyPlanned = false;
    for (int count : planned) anyPlanned = anyPlanned || count > 0;
    if (anyPlanned) {
        emit(now, r, EventType::HospitalsChecked, "Hospitals checked for you, nearest first:", lines);
        for (size_t i = 0; i < checks.size(); ++i) {
            if (planned[i] > 0) dispatchPatients(c, *hospitals.find(checks[i].hospital->id), planned[i], now);
        }
    }

    if (r.patientsAssigned == r.patientCount) {
        r.status = RequestStatus::InProgress;
        return;
    }

    remaining = r.patientCount - r.patientsAssigned;
    std::string text = "No hospital can take " + std::string(r.patientsAssigned > 0 ? "the remaining " : "") +
                       patientsText(remaining) + " right now.";
    r.status = RequestStatus::Waiting;
    c.waitUntil = now + config.retryMinutes;
    if (!anyPlanned) emitOnce(c, now, EventType::HospitalsChecked, "Hospitals checked for you, nearest first:", lines);
    emitOnce(c, now, EventType::Waiting,
             text + " Retrying every " + minutesText(config.retryMinutes) + " and whenever an ambulance or doctor is free.");
}

int DispatchEngine::dispatchPatients(Case& c, Hospital& hospital, int count, double now) {
    Request& r = *c.request;
    int minute = minuteOfDay(now);

    // Same route for every ambulance from this hospital; times relative to departure.
    DoublyLinkedList<Checkpoint> toScene = map.routeToSource(c.tree, hospital.landmarkId, 0.0);
    DoublyLinkedList<Checkpoint> toHospital = map.routeFromSource(c.tree, hospital.landmarkId, 0.0);
    std::vector<std::string> steps = firstAid.stepsFor(r.injuryCode, r.severity);

    int dispatched = 0;
    for (int i = 0; i < count; ++i) {
        Ambulance* ambulance = ambulances.selectAmbulance(r, hospital.id);
        if (ambulance == nullptr) break;
        int units = 0;
        Doctor* doctor = hospitals.reserveDoctor(hospital, r.requiredSpecialization, minute, r.severity, &units);
        if (doctor == nullptr) break;

        std::string error;
        Trip* trip = ambulances.startTrip(ambulance, r.id, hospital.id, toScene, toHospital, now,
                                          config.sceneMinutes, &error);
        if (trip == nullptr) {
            hospitals.scheduleRelease(doctor, now, units);  // give the load back
            break;
        }
        hospitals.scheduleRelease(doctor, trip->arriveHospitalAt + config.treatmentMinutes, units);
        ++r.patientsAssigned;
        ++dispatched;

        c.assignments.push_back(
            {ambulance->id, hospital.id, doctor->name, now, trip->arriveSceneAt, trip->arriveHospitalAt});

        std::vector<std::string> route;
        trip->route.forEach([&](const Checkpoint& checkpoint) {
            std::string marker = &checkpoint == &trip->scene->value ? "  <- patient pickup" : "";
            route.push_back(formatClock(checkpoint.atMinute) + "  " + checkpoint.name + marker);
        });
        EngineEvent& sent = emit(
            now, r, EventType::Dispatched,
            "Ambulance " + ambulance->id + " (" + toString(ambulance->type) + ") dispatched from " + hospital.name +
                ". Destination: " + hospital.name + ". Reaches you in about " + minutesText(trip->arriveSceneAt - now) +
                " (" + formatClock(trip->arriveSceneAt) + "). " + doctor->name + " (" +
                humanize(r.requiredSpecialization) + ") will be ready.",
            route);
        sent.hospitalId = hospital.id;
        sent.ambulanceId = ambulance->id;

        EngineEvent& aid = emit(now, r, EventType::FirstAid,
                                "First aid for ambulance " + ambulance->id + " crew - " + injuryName(r) + ", " +
                                    toString(r.severity) + ":",
                                steps, false);
        aid.hospitalId = hospital.id;
        aid.ambulanceId = ambulance->id;
    }
    return dispatched;
}

// ------------------------------------------------------------ trip progress

void DispatchEngine::onTripEvent(const TripEvent& event) {
    if (event.type == TripEventType::ReturnConfirmed || event.type == TripEventType::ReturnTimedOut) {
        Request fallback;  // the request may be unknown if it was never ours
        fallback.id = event.requestId;
        Case** found = cases.find(event.requestId);
        const Request& r = found == nullptr ? fallback : *(*found)->request;
        std::string how = event.type == TripEventType::ReturnConfirmed
                              ? "confirmed by the hospital"
                              : "counted back automatically, " + minutesText(config.returnTimeoutMinutes) +
                                    " after dispatch";
        EngineEvent& back = emit(event.time, r, EventType::AmbulanceReturned,
                                 "Ambulance " + event.ambulanceId + " is available again at " +
                                     hospitalName(event.hospitalId) + " (" + how + ").",
                                 {}, false);
        back.hospitalId = event.hospitalId;
        back.ambulanceId = event.ambulanceId;
        return;
    }

    Case** found = cases.find(event.requestId);
    if (found == nullptr) return;
    Case& c = **found;
    Request& r = *c.request;
    EngineEvent* out = nullptr;

    switch (event.type) {
        case TripEventType::ArrivedAtScene:
            out = &emit(event.time, r, EventType::ArrivedAtScene,
                        "Ambulance " + event.ambulanceId + " reached the patient at " + map.landmarkName(r.landmarkId) +
                            ". First aid started.");
            break;
        case TripEventType::LeftScene: {
            std::string eta;
            for (const Assignment& a : c.assignments) {
                if (a.ambulanceId == event.ambulanceId) eta = " (arrives " + formatClock(a.arriveHospitalAt) + ")";
            }
            out = &emit(event.time, r, EventType::LeftScene,
                        "Ambulance " + event.ambulanceId + " is taking the patient to " +
                            hospitalName(event.hospitalId) + eta + ".");
            break;
        }
        case TripEventType::ArrivedAtHospital: {
            ++r.patientsDelivered;
            out = &emit(event.time, r, EventType::ArrivedAtHospital,
                        "Ambulance " + event.ambulanceId + " reached " + hospitalName(event.hospitalId) +
                            ". Patient handed over to the doctor.");
            out->hospitalId = event.hospitalId;
            out->ambulanceId = event.ambulanceId;
            if (r.patientsDelivered == r.patientCount) {
                r.status = RequestStatus::Completed;
                emit(event.time, r, EventType::Completed,
                     r.patientCount == 1 ? "Patient reached the hospital. Request complete."
                                         : "All " + patientsText(r.patientCount) +
                                               " reached hospitals. Request complete.");
            }
            return;
        }
        default:
            return;
    }
    out->hospitalId = event.hospitalId;
    out->ambulanceId = event.ambulanceId;
}

// ------------------------------------------------------------ events and views

EngineEvent& DispatchEngine::emit(double time, const Request& request, EventType type, const std::string& text,
                                  const std::vector<std::string>& details, bool forPatient) {
    EngineEvent event;
    event.time = time;
    event.requestId = request.id;
    event.type = type;
    event.text = text;
    event.details = details;
    event.forPatient = forPatient;
    pending.push_back(event);
    return pending.back();
}

void DispatchEngine::emitOnce(Case& c, double time, EventType type, const std::string& text,
                              const std::vector<std::string>& details) {
    std::string key = toString(type) + "|" + text;
    for (const std::string& line : details) key += "|" + line;
    if (c.announced.contains(key)) return;
    c.announced.put(key, true);
    emit(time, *c.request, type, text, details);
}

std::vector<EngineEvent> DispatchEngine::takeEvents() {
    std::vector<EngineEvent> events;
    events.swap(pending);
    return events;
}

std::vector<const Request*> DispatchEngine::activeRequests() const {
    RequestQueue ordered;  // reuse the priority order for the dashboard
    for (Case* c : caseOrder) {
        if (c->request->status != RequestStatus::Completed) ordered.push(c->request);
    }
    std::vector<const Request*> result;
    while (!ordered.empty()) result.push_back(ordered.pop());
    return result;
}

int DispatchEngine::completedCount() const {
    int count = 0;
    for (const Case* c : caseOrder) {
        if (c->request->status == RequestStatus::Completed) ++count;
    }
    return count;
}

const Request* DispatchEngine::findRequest(const std::string& id) const {
    Case* const* found = cases.find(id);
    return found == nullptr ? nullptr : (*found)->request;
}

std::vector<Assignment> DispatchEngine::assignmentsFor(const std::string& requestId) const {
    Case* const* found = cases.find(requestId);
    return found == nullptr ? std::vector<Assignment>() : (*found)->assignments;
}

std::vector<std::string> DispatchEngine::hospitalCheckLines(const std::string& requestId) const {
    Case* const* found = cases.find(requestId);
    return found == nullptr ? std::vector<std::string>() : (*found)->checkLines;
}

double DispatchEngine::nextActionAt(const std::string& requestId) const {
    Case* const* found = cases.find(requestId);
    return found == nullptr ? 0 : (*found)->waitUntil;
}

std::string DispatchEngine::injuryName(const Request& request) const {
    const Injury* injury = injuries.find(request.injuryCode);
    return injury == nullptr ? request.injuryCode : injury->name;
}

std::string DispatchEngine::hospitalName(const std::string& hospitalId) const {
    const Hospital* hospital = hospitals.find(hospitalId);
    return hospital == nullptr ? hospitalId : hospital->name;
}

}  // namespace rahat
