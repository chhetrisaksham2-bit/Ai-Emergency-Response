#include "admin_server.h"

#include "protocol.h"

namespace rahat {

namespace {

const size_t kMaxLog = 200;

std::string severityName(const Request& r) { return toString(r.severity); }

}  // namespace

AdminServer::AdminServer(World& world, const std::string& sharedDir, EngineSettings settings)
    : world(world),
      channel(sharedDir),
      engine(world.map, world.injuries, world.firstAid, world.hospitals, world.ambulances, settings) {}

bool AdminServer::start(std::string* error) {
    std::vector<std::string> ids;
    for (const Hospital* hospital : world.hospitals.all()) ids.push_back(hospital->id);
    if (!channel.startSession(ids, &sessionId, error)) return false;
    sendStatuses();
    return true;
}

void AdminServer::addLog(double time, const std::string& line) {
    log.push_back("[" + formatClock(time) + "] " + line);
    if (log.size() > kMaxLog) log.erase(log.begin());
}

void AdminServer::toPatient(const std::string& requestId, double time, const std::string& type,
                            const std::string& text, const std::vector<std::string>& details,
                            const std::string& destination, const std::string& severity) {
    Message update;
    update.set("type", type);
    update.set("time", formatClock(time));
    update.set("text", text);
    update.set("details", protocol::joinLines(details));
    if (!destination.empty()) update.set("destination", destination);
    if (!severity.empty()) update.set("severity", severity);

    int* seq = patientSeq.find(requestId);
    if (seq == nullptr) {
        patientSeq.put(requestId, 0);
        seq = patientSeq.find(requestId);
    }
    std::string error;
    if (channel.sendUpdate(requestId, *seq, update, &error)) {
        ++*seq;
    } else {
        addLog(time, "Could not send update: " + error);
    }
}

void AdminServer::toHospital(const std::string& hospitalId, const Message& message) {
    int* seq = hospitalSeq.find(hospitalId);
    if (seq == nullptr) {
        hospitalSeq.put(hospitalId, 0);
        seq = hospitalSeq.find(hospitalId);
    }
    std::string error;
    if (channel.sendToHospital(hospitalId, *seq, message, &error)) {
        ++*seq;
    } else {
        log.push_back("Could not send to " + hospitalId + ": " + error);
    }
}

std::string AdminServer::statusText(const std::string& hospitalId) const {
    std::string text;
    for (const Ambulance* a : world.ambulances.all()) {
        if (a->hospitalId != hospitalId) continue;
        if (!text.empty()) text += ";";
        text += a->id + "," + toString(a->type) + "," + toString(a->status) + "," + a->requestId;
    }
    return text;
}

bool AdminServer::sendStatuses() {
    bool changed = false;
    for (const Hospital* hospital : world.hospitals.all()) {
        std::string text = statusText(hospital->id);
        const std::string* last = lastStatus.find(hospital->id);
        if (last != nullptr && *last == text) continue;
        lastStatus.put(hospital->id, text);
        Message status;
        status.set("kind", protocol::kStatus);
        status.set("name", hospital->name);
        status.set("ambulances", text);
        toHospital(hospital->id, status);
        changed = true;
    }
    return changed;
}

void AdminServer::routeEvent(const EngineEvent& event) {
    const Request* r = engine.findRequest(event.requestId);
    std::string who = protocol::shortId(event.requestId);
    addLog(event.time, who + " " + event.text);

    if (event.forPatient) {
        std::string destination;
        if (event.type == EventType::Dispatched) destination = engine.hospitalName(event.hospitalId);
        toPatient(event.requestId, event.time, toString(event.type), event.text, event.details, destination,
                  r == nullptr ? "" : severityName(*r));
    }
    if (event.hospitalId.empty()) return;

    if (event.type == EventType::Dispatched && r != nullptr) {
        Assignment assignment;
        for (const Assignment& a : engine.assignmentsFor(r->id)) {
            if (a.ambulanceId == event.ambulanceId) assignment = a;
        }
        const Ambulance* ambulance = world.ambulances.find(event.ambulanceId);
        Message m;
        m.set("kind", protocol::kAssignment);
        m.set("request", r->id);
        m.set("time", formatClock(event.time));
        m.set("ambulance", event.ambulanceId);
        m.set("ambulance_type", ambulance == nullptr ? "" : toString(ambulance->type));
        m.set("injury", engine.injuryName(*r));
        m.set("severity", severityName(*r));
        m.set("points", r->points);
        m.set("patients", r->patientCount);
        m.set("pickup", world.map.landmarkName(r->landmarkId));
        m.set("destination", engine.hospitalName(event.hospitalId));
        m.set("eta_scene", formatClock(assignment.arriveSceneAt));
        m.set("eta_hospital", formatClock(assignment.arriveHospitalAt));
        m.set("doctor", assignment.doctorName);
        m.set("route", protocol::joinLines(event.details));
        m.set("first_aid", protocol::joinLines(world.firstAid.stepsFor(r->injuryCode, r->severity)));
        toHospital(event.hospitalId, m);
        return;
    }
    if (event.type == EventType::ArrivedAtScene || event.type == EventType::LeftScene ||
        event.type == EventType::ArrivedAtHospital || event.type == EventType::AmbulanceReturned) {
        Message m;
        m.set("kind", protocol::kProgress);
        m.set("request", event.requestId);
        m.set("ambulance", event.ambulanceId);
        m.set("type", toString(event.type));
        m.set("time", formatClock(event.time));
        m.set("text", event.text);
        toHospital(event.hospitalId, m);
    }
}

bool AdminServer::step(double now, double speed) {
    bool changed = false;
    std::string error;

    // 1. New requests.
    std::vector<std::string> problems;
    for (const Message& message : channel.collectRequests(&problems)) {
        changed = true;
        Request* request = new Request();
        std::string reason;
        if (!messageToRequest(message, request, &reason) || !engine.submit(request, now, &reason)) {
            std::string id = message.get("id", "");
            addLog(now, "Rejected request " + (id.empty() ? std::string("(no id)") : protocol::shortId(id)) + ": " +
                            reason);
            if (!id.empty()) toPatient(id, now, protocol::kInvalid, "Request could not be accepted: " + reason, {});
            delete request;
        }
    }
    for (const std::string& problem : problems) addLog(now, "Bad request file: " + problem);

    // 2. Hospital buttons.
    for (const Hospital* hospital : world.hospitals.all()) {
        problems.clear();
        for (const Message& message : channel.collectFromHospital(hospital->id, &problems)) {
            changed = true;
            if (message.get("kind") != protocol::kReturned) continue;
            std::string ambulanceId = message.get("ambulance");
            Message reply;
            reply.set("kind", protocol::kResult);
            reply.set("ambulance", ambulanceId);
            if (engine.confirmReturn(hospital->id, ambulanceId, now, &error)) {
                reply.set("ok", 1);
                reply.set("text", "Ambulance " + ambulanceId + " is available again.");
            } else {
                reply.set("ok", 0);
                reply.set("text", error + ".");
                addLog(now, hospital->name + ": return of " + ambulanceId + " refused - " + error);
            }
            toHospital(hospital->id, reply);
        }
        for (const std::string& problem : problems) addLog(now, hospital->id + " sent a bad file: " + problem);
    }

    // 3 and 4. Run the engine and deliver what happened.
    engine.tick(now);
    for (const EngineEvent& event : engine.takeEvents()) {
        changed = true;
        routeEvent(event);
    }

    // 5. Ambulance counts for each hospital, and the clock for everyone.
    if (sendStatuses()) changed = true;
    channel.writeClock(now, speed, &error);
    return changed;
}

AdminView AdminServer::view(double now) const {
    AdminView v;
    v.clock = formatClock(now);
    double servesAt = 0;
    if (engine.batchWaiting(now, &servesAt)) v.batch = "collecting new requests until " + formatClock(servesAt);

    for (const Request* r : engine.activeRequests()) {
        AdminView::RequestRow row;
        row.id = r->id;
        row.injury = engine.injuryName(*r);
        row.landmark = world.map.landmarkName(r->landmarkId);
        row.severity = toString(r->severity);
        row.priorityScore = r->priorityScore;
        row.patients = r->patientCount;
        row.status = toString(r->status);
        for (const Assignment& a : engine.assignmentsFor(r->id)) {
            std::string name = engine.hospitalName(a.hospitalId);
            if (row.hospitals.find(name) == std::string::npos) row.hospitals += (row.hospitals.empty() ? "" : ", ") + name;
        }
        row.checks = engine.hospitalCheckLines(r->id);
        v.active.push_back(row);
    }
    v.completed = engine.completedCount();

    for (const Hospital* hospital : world.hospitals.all()) {
        AdminView::HospitalRow row;
        row.id = hospital->id;
        row.name = hospital->name;
        for (const Ambulance* a : world.ambulances.all()) {
            if (a->hospitalId != hospital->id) continue;
            ++row.total;
            if (a->status == AmbulanceStatus::Available) (a->type == AmbulanceType::ALS ? row.freeAls : row.freeBls)++;
        }
        v.hospitals.push_back(row);
    }
    for (const Ambulance* a : world.ambulances.all()) {
        v.ambulances.push_back({a->id, a->hospitalId, toString(a->type), toString(a->status), a->requestId});
    }
    v.log = log;
    return v;
}

}  // namespace rahat
