// First-aid module: the steps an ambulance crew follows for an injury.
//
// Steps are stored per injury for "ANY" severity (always shown) and per
// specific severity (shown after the general steps).
#pragma once

#include <string>
#include <vector>

#include "../ds/hash_map.h"
#include "../models/models.h"

namespace rahat {

class FirstAidGuide {
public:
    // severity is "ANY" or MINOR / MODERATE / SERIOUS / CRITICAL.
    // Returns false if the severity text is not recognised.
    bool addStep(const std::string& injuryCode, const std::string& severity, const std::string& instruction);

    // General steps for the injury, followed by the steps for this severity.
    std::vector<std::string> stepsFor(const std::string& injuryCode, Severity severity) const;

    bool hasStepsFor(const std::string& injuryCode) const;

private:
    HashMap<std::string, std::vector<std::string>> steps;  // key: "INJURY|SEVERITY"

    static std::string keyOf(const std::string& injuryCode, const std::string& severity);
};

}  // namespace rahat
