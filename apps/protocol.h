// The messages the RAHAT windows send each other (see modules/comms for the folders).
//
// Patient / generator -> admin   requests/req_<id>.txt
//   id, injury, answers, landmark, patients            (see requestToMessage)
//
// Admin -> patient               updates/<id>_<seq>.txt
//   type         RECEIVED, HOSPITALS_CHECKED, DISPATCHED, ARRIVED_AT_SCENE, LEFT_SCENE,
//                ARRIVED_AT_HOSPITAL, COMPLETED, WAITING or INVALID
//   time         simulated time, HH:MM
//   text         one line for people
//   details      extra lines (hospital list, route), separated by newlines
//   destination  DISPATCHED only: the hospital the patient is taken to
//   severity     the request's severity (MINOR ... CRITICAL)
//
// Admin -> hospital H            hospitals/H/from_admin/<seq>.txt
//   kind=STATUS      name, ambulances = "A1,ALS,AVAILABLE,<request>;A2,..."
//   kind=ASSIGNMENT  request, ambulance, ambulance_type, injury, severity, points, patients,
//                    pickup (patient's landmark = pickup point), destination, eta_scene,
//                    eta_hospital, doctor, route (lines), first_aid (lines), time
//   kind=PROGRESS    request, ambulance, type, time, text
//   kind=RESULT      ok (1 / 0), text: the answer to a button press
//
// Hospital H -> admin            hospitals/H/to_admin/msg_<id>.txt
//   kind=RETURNED    ambulance: the "ambulance is back" button
#pragma once

#include <string>
#include <vector>

namespace rahat {
namespace protocol {

const char* const kStatus = "STATUS";
const char* const kAssignment = "ASSIGNMENT";
const char* const kProgress = "PROGRESS";
const char* const kResult = "RESULT";
const char* const kReturned = "RETURNED";
const char* const kInvalid = "INVALID";

inline std::string joinLines(const std::vector<std::string>& lines) {
    std::string text;
    for (size_t i = 0; i < lines.size(); ++i) text += (i == 0 ? "" : "\n") + lines[i];
    return text;
}

inline std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    if (text.empty()) return lines;
    size_t start = 0;
    while (true) {
        size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return lines;
}

inline std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    if (text.empty()) return parts;
    size_t start = 0;
    while (true) {
        size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return parts;
}

// "R1757925123456-042" -> "...6-042", short enough for a table.
inline std::string shortId(const std::string& id) {
    return id.size() <= 8 ? id : "..." + id.substr(id.size() - 5);
}

}  // namespace protocol
}  // namespace rahat
