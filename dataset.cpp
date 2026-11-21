#include "dataset.h"

#include <algorithm>

#include "csv.h"

namespace phase2 {

// ---------------------------------------------------------------- small helpers

std::string toString(Severity severity) {
    switch (severity) {
        case Severity::Minor: return "MINOR";
        case Severity::Moderate: return "MODERATE";
        case Severity::Serious: return "SERIOUS";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

bool parseSeverity(const std::string& text, Severity* out) {
    if (text == "MINOR") *out = Severity::Minor;
    else if (text == "MODERATE") *out = Severity::Moderate;
    else if (text == "SERIOUS") *out = Severity::Serious;
    else if (text == "CRITICAL") *out = Severity::Critical;
    else return false;
    return true;
}

const Landmark* Dataset::findLandmark(int id) const {
    for (const Landmark& landmark : landmarks) {
        if (landmark.id == id) return &landmark;
    }
    return nullptr;
}

std::string Dataset::landmarkName(int id) const {
    const Landmark* landmark = findLandmark(id);
    return landmark == nullptr ? "Unknown place" : landmark->name;
}

namespace {

std::string where(const std::string& file, size_t row) { return file + " row " + std::to_string(row + 1) + ": "; }

bool toInt(const std::string& text, int* out) {
    try {
        size_t used = 0;
        *out = std::stoi(text, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

bool toDouble(const std::string& text, double* out) {
    try {
        size_t used = 0;
        *out = std::stod(text, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

// Opens a file and finds the needed columns. columns[i] gets the position of names[i].
bool openTable(const std::string& dir, const std::string& file, const std::vector<std::string>& names,
               CsvTable* table, std::vector<int>* columns, std::string* error) {
    if (!readCsv(dir + "/" + file, table, error)) return false;
    columns->clear();
    for (const std::string& name : names) {
        int position = table->column(name);
        if (position < 0) {
            *error = file + " is missing the column '" + name + "'";
            return false;
        }
        columns->push_back(position);
    }
    return true;
}

Injury* findInjury(Dataset* data, const std::string& code) {
    for (Injury& injury : data->injuries) {
        if (injury.code == code) return &injury;
    }
    return nullptr;
}

// ---------------------------------------------------------------- one loader per file

bool loadLandmarks(const std::string& dir, Dataset* data, std::string* error) {
    CsvTable t;
    std::vector<int> c;
    if (!openTable(dir, "landmarks.csv", {"id", "name"}, &t, &c, error)) return false;
    for (size_t r = 0; r < t.rows.size(); ++r) {
        Landmark landmark;
        landmark.name = t.get(r, c[1]);
        if (!toInt(t.get(r, c[0]), &landmark.id)) {
            *error = where("landmarks.csv", r) + "id must be a whole number";
            return false;
        }
        if (data->findLandmark(landmark.id) != nullptr) {
            *error = where("landmarks.csv", r) + "id " + std::to_string(landmark.id) + " is used twice";
            return false;
        }
        data->landmarks.push_back(landmark);
    }
    return true;
}

bool loadRoads(const std::string& dir, Dataset* data, std::string* error) {
    CsvTable t;
    std::vector<int> c;
    if (!openTable(dir, "roads.csv", {"id", "from", "to", "distance_km"}, &t, &c, error)) return false;
    for (size_t r = 0; r < t.rows.size(); ++r) {
        Road road;
        road.id = t.get(r, c[0]);
        if (!toInt(t.get(r, c[1]), &road.from) || !toInt(t.get(r, c[2]), &road.to) ||
            !toDouble(t.get(r, c[3]), &road.km)) {
            *error = where("roads.csv", r) + "from, to and distance_km must be numbers";
            return false;
        }
        if (!data->findLandmark(road.from) || !data->findLandmark(road.to)) {
            *error = where("roads.csv", r) + "road " + road.id + " uses a landmark that is not in landmarks.csv";
            return false;
        }
        if (road.from == road.to || road.km <= 0) {
            *error = where("roads.csv", r) + "road " + road.id + " must join two different places with km > 0";
            return false;
        }
        data->roads.push_back(road);
    }
    return true;
}

bool loadHospitals(const std::string& dir, Dataset* data, std::string* error) {
    CsvTable t;
    std::vector<int> c;
    if (!openTable(dir, "hospitals.csv", {"id", "name", "landmark", "facilities"}, &t, &c, error)) return false;
    for (size_t r = 0; r < t.rows.size(); ++r) {
        Hospital hospital;
        hospital.id = t.get(r, c[0]);
        hospital.name = t.get(r, c[1]);
        hospital.facilities = splitList(t.get(r, c[3]));
        if (!toInt(t.get(r, c[2]), &hospital.landmarkId) || !data->findLandmark(hospital.landmarkId)) {
            *error = where("hospitals.csv", r) + hospital.id + " must stand at a landmark from landmarks.csv";
            return false;
        }
        data->hospitals.push_back(hospital);
    }
    return true;
}

bool loadInjuries(const std::string& dir, Dataset* data, std::string* error) {
    CsvTable t;
    std::vector<int> c;
    if (!openTable(dir, "injuries.csv", {"code", "name"}, &t, &c, error)) return false;
    for (size_t r = 0; r < t.rows.size(); ++r) {
        Injury injury;
        injury.code = t.get(r, c[0]);
        injury.name = t.get(r, c[1]);
        data->injuries.push_back(injury);
    }
    return true;
}

bool loadQuestions(const std::string& dir, Dataset* data, std::string* error) {
    CsvTable t;
    std::vector<int> c;
    if (!openTable(dir, "injury_questions.csv", {"injury", "question", "option", "points"}, &t, &c, error)) {
        return false;
    }
    for (size_t r = 0; r < t.rows.size(); ++r) {
        Injury* injury = findInjury(data, t.get(r, c[0]));
        if (injury == nullptr) {
            *error = where("injury_questions.csv", r) + "unknown injury '" + t.get(r, c[0]) + "'";
            return false;
        }
        // Rows with the same question text, one after another, are the answers to one question.
        std::string text = t.get(r, c[1]);
        if (injury->questions.empty() || injury->questions.back().text != text) {
            injury->questions.push_back({text, {}});
        }
        AnswerOption option;
        option.text = t.get(r, c[2]);
        if (!toInt(t.get(r, c[3]), &option.points)) {
            *error = where("injury_questions.csv", r) + "points must be a whole number";
            return false;
        }
        injury->questions.back().options.push_back(option);
    }
    return true;
}

bool loadRules(const std::string& dir, Dataset* data, std::string* error) {
    CsvTable t;
    std::vector<int> c;
    if (!openTable(dir, "severity_rules.csv", {"injury", "severity", "min_points", "facilities", "ambulance"}, &t,
                   &c, error)) {
        return false;
    }
    for (size_t r = 0; r < t.rows.size(); ++r) {
        Injury* injury = findInjury(data, t.get(r, c[0]));
        SeverityRule rule;
        if (injury == nullptr || !parseSeverity(t.get(r, c[1]), &rule.severity)) {
            *error = where("severity_rules.csv", r) + "unknown injury or severity";
            return false;
        }
        if (!toInt(t.get(r, c[2]), &rule.minPoints)) {
            *error = where("severity_rules.csv", r) + "min_points must be a whole number";
            return false;
        }
        rule.facilities = splitList(t.get(r, c[3]));
        rule.ambulance = t.get(r, c[4]);
        if (rule.ambulance != "BLS" && rule.ambulance != "ALS") {
            *error = where("severity_rules.csv", r) + "ambulance must be BLS or ALS";
            return false;
        }
        injury->rules.push_back(rule);
    }
    return true;
}

// Final checks that need all files loaded.
bool checkInjuries(Dataset* data, std::string* error) {
    for (Injury& injury : data->injuries) {
        std::sort(injury.rules.begin(), injury.rules.end(),
                  [](const SeverityRule& a, const SeverityRule& b) { return a.minPoints < b.minPoints; });
        if (injury.questions.empty()) {
            *error = injury.code + " has no questions in injury_questions.csv";
            return false;
        }
        for (const Question& q : injury.questions) {
            if (q.options.size() < 2) {
                *error = injury.code + ": question \"" + q.text + "\" needs at least 2 answers";
                return false;
            }
        }
        if (injury.rules.empty() || injury.rules.front().minPoints != 0) {
            *error = injury.code + " needs a severity rule with min_points 0 in severity_rules.csv";
            return false;
        }
    }
    return true;
}

}  // namespace

bool loadDataset(const std::string& dataDir, Dataset* dataset, std::string* error) {
    *dataset = Dataset();
    // Order matters: roads and hospitals need landmarks; questions and rules need injuries.
    return loadLandmarks(dataDir, dataset, error) && loadRoads(dataDir, dataset, error) &&
           loadHospitals(dataDir, dataset, error) && loadInjuries(dataDir, dataset, error) &&
           loadQuestions(dataDir, dataset, error) && loadRules(dataDir, dataset, error) &&
           checkInjuries(dataset, error);
}

}  // namespace phase2
