// Request generator: makes requests for demos and tests, without the patient window.
//
// Scenario file (CSV, one request per row):
//   send_at,landmark,injury,patients,answers
//   0,Clock Tower,STROKE,1,Yes|Yes|Yes|Unconscious|Less than 4.5 hours ago
//
//   send_at   seconds (real time) after the generator starts
//   landmark  name or id from landmarks.csv
//   injury    code or name from injuries.csv
//   patients  number of patients at that place
//   answers   one answer per follow-up question, in order, separated by |
//             Each answer is the option text, or its number (1 = first option).
//
// Random mode picks the injury, answers and landmark at random from the data,
// with a seed so the same seed always gives the same requests.
#pragma once

#include <random>
#include <string>
#include <vector>

#include "../modules/data_io/data_io.h"

namespace rahat {

struct ScenarioItem {
    double sendAt = 0;  // real seconds after start
    Request request;    // id is filled when it is sent
    std::string label;  // "Stroke at Clock Tower"
};

bool loadScenario(const std::string& path, const World& world, std::vector<ScenarioItem>* items,
                  std::vector<std::string>* problems);

ScenarioItem randomItem(const World& world, std::mt19937& random, double sendAt);

}  // namespace rahat
