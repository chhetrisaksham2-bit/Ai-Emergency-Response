#include "hospital_client.h"

#include "../modules/models/models.h"
#include "protocol.h"

namespace rahat {

HospitalClient::HospitalClient(const std::string& sharedDir, const std::string& hospitalId)
    : channel(sharedDir), hospitalId(hospitalId), hospitalName(hospitalId) {}

void HospitalClient::reset() {
    nextSeq = 0;
    list.clear();
    fleet.clear();
    result.clear();
    resultOk = true;
}

bool HospitalClient::poll() {
    bool changed = false;
    std::string current = channel.currentSession();
    if (current != session) {
        session = current;
        reset();
        changed = true;
    }

    std::string clockNow = clock();
    if (clockNow != lastClock) {
        lastClock = clockNow;
        changed = true;
    }
    if (session.empty()) return changed;

    for (const Message& message : channel.readFromAdmin(hospitalId, &nextSeq)) {
        apply(message);
        changed = true;
    }
    return changed;
}

HospitalAssignment* HospitalClient::find(const std::string& requestId, const std::string& ambulanceId) {
    for (HospitalAssignment& a : list) {
        if (a.requestId == requestId && a.ambulanceId == ambulanceId) return &a;
    }
    return nullptr;
}

void HospitalClient::apply(const Message& message) {
    std::string kind = message.get("kind");

    if (kind == protocol::kStatus) {
        hospitalName = message.get("name", hospitalId);
        fleet.clear();
        for (const std::string& item : protocol::split(message.get("ambulances"), ';')) {
            std::vector<std::string> parts = protocol::split(item, ',');
            HospitalAmbulance a;
            if (parts.size() > 0) a.id = parts[0];
            if (parts.size() > 1) a.type = parts[1];
            if (parts.size() > 2) a.status = parts[2];
            if (parts.size() > 3) a.requestId = parts[3];
            fleet.push_back(a);
        }
    } else if (kind == protocol::kAssignment) {
        HospitalAssignment a;
        a.requestId = message.get("request");
        a.ambulanceId = message.get("ambulance");
        a.ambulanceType = message.get("ambulance_type");
        a.injury = message.get("injury");
        a.severity = message.get("severity");
        a.points = message.getInt("points", 0);
        a.patients = message.getInt("patients", 1);
        a.pickup = message.get("pickup");
        a.destination = message.get("destination");
        a.dispatchedAt = message.get("time");
        a.etaScene = message.get("eta_scene");
        a.etaHospital = message.get("eta_hospital");
        a.doctor = message.get("doctor");
        a.route = protocol::splitLines(message.get("route"));
        a.firstAid = protocol::splitLines(message.get("first_aid"));
        a.timeline.push_back(a.dispatchedAt + "  Ambulance " + a.ambulanceId + " dispatched to pick up the patient at " +
                             a.pickup + ".");
        list.insert(list.begin(), a);  // newest first
    } else if (kind == protocol::kProgress) {
        HospitalAssignment* a = find(message.get("request"), message.get("ambulance"));
        std::string type = message.get("type");
        if (a == nullptr && type == "AMBULANCE_RETURNED") {
            // Counted back for a request this window did not see (e.g. restarted window): find by ambulance.
            for (HospitalAssignment& candidate : list) {
                if (candidate.ambulanceId == message.get("ambulance") && !candidate.returned) a = &candidate;
            }
        }
        if (a != nullptr) {
            a->timeline.push_back(message.get("time") + "  " + message.get("text"));
            a->stage = type;
            if (type == "AMBULANCE_RETURNED") a->returned = true;
        }
    } else if (kind == protocol::kResult) {
        result = message.get("text");
        resultOk = message.get("ok") == "1";
    }
}

bool HospitalClient::pressReturned(const std::string& ambulanceId, std::string* error) {
    if (session.empty()) {
        *error = "The admin window is not running";
        result = *error;
        resultOk = false;
        return false;
    }
    Message message;
    message.set("kind", protocol::kReturned);
    message.set("ambulance", ambulanceId);
    if (!channel.sendToAdmin(hospitalId, message, error)) {
        result = *error;
        resultOk = false;
        return false;
    }
    result = "Sent: ambulance " + ambulanceId + " is back. Waiting for the admin...";
    resultOk = true;
    return true;
}

int HospitalClient::freeAmbulances() const {
    int count = 0;
    for (const HospitalAmbulance& a : fleet) {
        if (a.status == "AVAILABLE") ++count;
    }
    return count;
}

std::string HospitalClient::clock() const {
    double minutes = 0, speed = 0;
    if (!channel.readClock(&minutes, &speed)) return "--:--";
    return formatClock(minutes);
}

}  // namespace rahat
