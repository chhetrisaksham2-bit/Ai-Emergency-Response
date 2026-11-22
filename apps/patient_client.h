// PatientClient: sends one request and follows its updates, without the screen.
// Used by the patient window; the generator only uses send().
#pragma once

#include <string>
#include <vector>

#include "../modules/comms/comms.h"

namespace rahat {

struct PatientUpdate {
    std::string type;
    std::string time;
    std::string text;
    std::vector<std::string> details;
};

class PatientClient {
public:
    explicit PatientClient(const std::string& sharedDir);

    bool adminRunning() const { return !channel.currentSession().empty(); }

    // Sends a new request. Fills requestId. Fails if the admin window is not running.
    bool send(const Request& request, std::string* requestId, std::string* error);

    // Reads new updates for the request. Returns true if anything changed.
    bool poll();

    bool tracking() const { return !id.empty(); }
    const std::string& requestId() const { return id; }
    const std::vector<PatientUpdate>& updates() const { return log; }
    const std::vector<std::string>& hospitalsChecked() const { return checked; }  // latest list
    const std::string& destination() const { return destinationName; }
    const std::vector<std::string>& route() const { return routeLines; }  // "HH:MM  Landmark" lines
    const std::string& severity() const { return severityText; }
    bool finished() const { return done; }
    bool lost() const { return sessionLost; }  // the admin restarted, so this request is gone
    std::string clock() const;
    void forget();  // stop tracking, ready for a new request

private:
    Channel channel;
    std::string id;
    std::string session;
    int nextSeq = 0;
    std::vector<PatientUpdate> log;
    std::vector<std::string> checked;
    std::string destinationName;
    std::vector<std::string> routeLines;
    std::string severityText;
    bool done = false;
    bool sessionLost = false;
    std::string lastClock;
};

}  // namespace rahat
