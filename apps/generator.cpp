#include "generator.h"

#include <algorithm>
#include <cctype>

#include "protocol.h"

namespace rahat {

namespace {

std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string trim(const std::string& text) {
    size_t start = text.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = text.find_last_not_of(" \t");
    return text.substr(start, end - start + 1);
}

bool toNumber(const std::string& text, double* out) {
    try {
        size_t used = 0;
        *out = std::stod(text, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

int findLandmark(const World& world, const std::string& text) {
    double number = 0;
    if (toNumber(text, &number) && world.map.hasLandmark(static_cast<int>(number))) return static_cast<int>(number);
    for (int id : world.map.landmarkIds()) {
        if (lower(world.map.landmarkName(id)) == lower(text)) return id;
    }
    return -1;
}

const Injury* findInjury(const World& world, const std::string& text) {
    for (const Injury* injury : world.injuries.all()) {
        if (lower(injury->code) == lower(text) || lower(injury->name) == lower(text)) return injury;
    }
    return nullptr;
}

std::string labelOf(const World& world, const Injury& injury, int landmark, int patients) {
    std::string label = injury.name + " at " + world.map.landmarkName(landmark);
    if (patients > 1) label += " (" + std::to_string(patients) + " patients)";
    return label;
}

}  // namespace

bool loadScenario(const std::string& path, const World& world, std::vector<ScenarioItem>* items,
                  std::vector<std::string>* problems) {
    CsvTable table;
    std::string error;
    if (!readCsv(path, {"send_at", "landmark", "injury", "patients", "answers"}, &table, &error)) {
        problems->push_back(error);
        return false;
    }
    int cSend = table.column("send_at"), cLandmark = table.column("landmark"), cInjury = table.column("injury");
    int cPatients = table.column("patients"), cAnswers = table.column("answers");

    for (size_t r = 0; r < table.rows.size(); ++r) {
        std::string where = table.where(r) + ": ";
        ScenarioItem item;
        double patients = 0;
        if (!toNumber(trim(table.value(r, cSend)), &item.sendAt) || item.sendAt < 0) {
            problems->push_back(where + "send_at must be a number of seconds (0 or more)");
            continue;
        }
        int landmark = findLandmark(world, trim(table.value(r, cLandmark)));
        if (landmark < 0) {
            problems->push_back(where + "unknown landmark '" + table.value(r, cLandmark) + "'");
            continue;
        }
        const Injury* injury = findInjury(world, trim(table.value(r, cInjury)));
        if (injury == nullptr) {
            problems->push_back(where + "unknown injury '" + table.value(r, cInjury) + "'");
            continue;
        }
        if (!toNumber(trim(table.value(r, cPatients)), &patients) || patients < 1) {
            problems->push_back(where + "patients must be 1 or more");
            continue;
        }

        std::vector<std::string> answers = protocol::split(table.value(r, cAnswers), '|');
        if (answers.size() != injury->questions.size()) {
            problems->push_back(where + injury->name + " has " + std::to_string(injury->questions.size()) +
                                " questions, but " + std::to_string(answers.size()) + " answers were given");
            continue;
        }
        bool ok = true;
        for (size_t q = 0; q < answers.size() && ok; ++q) {
            const Question& question = injury->questions[q];
            std::string answer = trim(answers[q]);
            int chosen = -1;
            double number = 0;
            if (toNumber(answer, &number) && number >= 1 && number <= question.options.size()) {
                chosen = static_cast<int>(number) - 1;
            }
            for (size_t o = 0; o < question.options.size() && chosen < 0; ++o) {
                if (lower(question.options[o].text) == lower(answer)) chosen = static_cast<int>(o);
            }
            if (chosen < 0) {
                std::string choices;
                for (size_t o = 0; o < question.options.size(); ++o) {
                    choices += (o == 0 ? "" : " / ") + question.options[o].text;
                }
                problems->push_back(where + "'" + answer + "' is not an answer to \"" + question.text +
                                    "\" (choose: " + choices + ")");
                ok = false;
            }
            item.request.answers.push_back(chosen);
        }
        if (!ok) continue;

        item.request.injuryCode = injury->code;
        item.request.landmarkId = landmark;
        item.request.patientCount = static_cast<int>(patients);
        item.label = labelOf(world, *injury, landmark, item.request.patientCount);
        items->push_back(item);
    }
    std::stable_sort(items->begin(), items->end(),
                     [](const ScenarioItem& a, const ScenarioItem& b) { return a.sendAt < b.sendAt; });
    return problems->empty();
}

ScenarioItem randomItem(const World& world, std::mt19937& random, double sendAt) {
    const std::vector<Injury*>& injuries = world.injuries.all();
    const std::vector<int>& landmarks = world.map.landmarkIds();
    const Injury& injury = *injuries[random() % injuries.size()];

    ScenarioItem item;
    item.sendAt = sendAt;
    item.request.injuryCode = injury.code;
    for (const Question& q : injury.questions) {
        item.request.answers.push_back(static_cast<int>(random() % q.options.size()));
    }
    item.request.landmarkId = landmarks[random() % landmarks.size()];
    item.request.patientCount = random() % 10 == 0 ? 2 : 1;  // sometimes two patients at one place
    item.label = labelOf(world, injury, item.request.landmarkId, item.request.patientCount);
    return item;
}

}  // namespace rahat
