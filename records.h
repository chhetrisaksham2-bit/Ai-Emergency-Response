// The records that come out of the data files.
// Owner: Abhinav (Dataset)
//
// Each struct matches one CSV file (or a group of rows in one file).
// Other parts only READ these records; only the dataset part creates them.
#pragma once

#include <string>
#include <vector>

namespace phase2 {

// ---------------------------------------------------------------- map

struct Landmark {          // landmarks.csv: id,name
    int id = 0;
    std::string name;
};

struct Road {              // roads.csv: id,from,to,distance_km  (Phase 2 ignores speed and traffic)
    std::string id;
    int from = 0;
    int to = 0;
    double km = 0;
};

// ---------------------------------------------------------------- hospitals

struct Hospital {          // hospitals.csv: id,name,landmark,facilities
    std::string id;
    std::string name;
    int landmarkId = 0;
    std::vector<std::string> facilities;  // e.g. ICU, CT_SCAN
};

// ---------------------------------------------------------------- injuries and severity

enum class Severity { Minor, Moderate, Serious, Critical };
std::string toString(Severity severity);                 // "MINOR", ...
bool parseSeverity(const std::string& text, Severity* out);

struct AnswerOption {      // injury_questions.csv: one row = one answer
    std::string text;
    int points = 0;
};

struct Question {          // rows with the same question text
    std::string text;
    std::vector<AnswerOption> options;
};

struct SeverityRule {      // severity_rules.csv: injury,severity,min_points,facilities,ambulance
    Severity severity = Severity::Minor;
    int minPoints = 0;
    std::vector<std::string> facilities;
    std::string ambulance;  // BLS or ALS
};

struct Injury {            // injuries.csv: code,name  (+ its questions and rules)
    std::string code;
    std::string name;
    std::vector<Question> questions;
    std::vector<SeverityRule> rules;  // sorted by minPoints, smallest first
};

// ---------------------------------------------------------------- everything together

struct Dataset {
    std::vector<Landmark> landmarks;
    std::vector<Road> roads;
    std::vector<Hospital> hospitals;
    std::vector<Injury> injuries;

    const Landmark* findLandmark(int id) const;
    std::string landmarkName(int id) const;  // "Unknown place" if missing
};

}  // namespace phase2
