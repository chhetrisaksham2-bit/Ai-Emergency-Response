#include "patient_client.h"

#include "protocol.h"

namespace rahat {

PatientClient::PatientClient(const std::string& sharedDir) : channel(sharedDir) {}

bool PatientClient::send(const Request& request, std::string* requestId, std::string* error) {
    session = channel.currentSession();
    if (session.empty()) {
        *error = "The admin window is not running. Start it first: ./build/admin";
        return false;
    }
    Request copy = request;
    copy.id = makeRequestId();
    if (!channel.sendRequest(copy.id, requestToMessage(copy), error)) return false;

    forget();
    id = copy.id;
    *requestId = copy.id;
    return true;
}

void PatientClient::forget() {
    id.clear();
    nextSeq = 0;
    log.clear();
    checked.clear();
    destinationName.clear();
    routeLines.clear();
    severityText.clear();
    done = false;
    sessionLost = false;
}

bool PatientClient::poll() {
    bool changed = false;
    std::string clockNow = clock();
    if (clockNow != lastClock) {
        lastClock = clockNow;
        changed = true;
    }
    if (id.empty() || sessionLost) return changed;

    if (channel.currentSession() != session) {
        sessionLost = true;
        return true;
    }
    for (const Message& m : channel.readUpdates(id, &nextSeq)) {
        PatientUpdate update{m.get("type"), m.get("time"), m.get("text"), protocol::splitLines(m.get("details"))};
        if (update.type == "HOSPITALS_CHECKED") checked = update.details;
        if (update.type == "DISPATCHED" && destinationName.empty()) {
            destinationName = m.get("destination");
            routeLines = update.details;
        }
        if (m.has("severity")) severityText = m.get("severity");
        if (update.type == "COMPLETED" || update.type == protocol::kInvalid) done = true;
        log.push_back(update);
        changed = true;
    }
    return changed;
}

std::string PatientClient::clock() const {
    double minutes = 0, speed = 0;
    if (!channel.readClock(&minutes, &speed)) return "--:--";
    return formatClock(minutes);
}

}  // namespace rahat
