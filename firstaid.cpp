#include "firstaid.h"

namespace rahat {

std::string FirstAidGuide::keyOf(const std::string& injuryCode, const std::string& severity) {
    return injuryCode + "|" + severity;
}

bool FirstAidGuide::addStep(const std::string& injuryCode, const std::string& severity,
                            const std::string& instruction) {
    Severity parsed;
    if (severity != "ANY" && !parseSeverity(severity, &parsed)) return false;

    std::string key = keyOf(injuryCode, severity);
    std::vector<std::string>* list = steps.find(key);
    if (list == nullptr) {
        steps.put(key, {instruction});
    } else {
        list->push_back(instruction);
    }
    return true;
}

std::vector<std::string> FirstAidGuide::stepsFor(const std::string& injuryCode, Severity severity) const {
    std::vector<std::string> result;
    if (const std::vector<std::string>* general = steps.find(keyOf(injuryCode, "ANY"))) {
        result.insert(result.end(), general->begin(), general->end());
    }
    if (const std::vector<std::string>* specific = steps.find(keyOf(injuryCode, toString(severity)))) {
        result.insert(result.end(), specific->begin(), specific->end());
    }
    return result;
}

bool FirstAidGuide::hasStepsFor(const std::string& injuryCode) const {
    for (const char* level : {"ANY", "MINOR", "MODERATE", "SERIOUS", "CRITICAL"}) {
        if (steps.contains(keyOf(injuryCode, level))) return true;
    }
    return false;
}

}  // namespace rahat
