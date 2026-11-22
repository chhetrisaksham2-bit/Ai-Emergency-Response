#include "triage.h"

#include <utility>

namespace rahat {

void Injury::addRule(const SeverityRule& rule) {
    // Insertion sort by minPoints so ruleFor() can scan in order.
    rules.push_back(rule);
    for (size_t i = rules.size() - 1; i > 0 && rules[i - 1].minPoints > rules[i].minPoints; --i) {
        std::swap(rules[i - 1], rules[i]);
    }
}

const SeverityRule* Injury::ruleFor(int points) const {
    const SeverityRule* match = nullptr;
    for (const SeverityRule& rule : rules) {
        if (rule.minPoints <= points) match = &rule;
    }
    return match;
}

InjuryCatalog::~InjuryCatalog() {
    for (Injury* injury : ordered) delete injury;
}

Injury* InjuryCatalog::addInjury(const std::string& code, const std::string& name, int basePriority) {
    if (byCode.contains(code)) return nullptr;
    Injury* injury = new Injury();
    injury->code = code;
    injury->name = name;
    injury->basePriority = basePriority;
    byCode.put(code, injury);
    ordered.push_back(injury);
    return injury;
}

Injury* InjuryCatalog::find(const std::string& code) {
    Injury** found = byCode.find(code);
    return found == nullptr ? nullptr : *found;
}

const Injury* InjuryCatalog::find(const std::string& code) const {
    Injury* const* found = byCode.find(code);
    return found == nullptr ? nullptr : *found;
}

bool InjuryCatalog::validate(std::vector<std::string>* problems) const {
    size_t before = problems->size();
    for (const Injury* injury : ordered) {
        if (injury->questions.empty()) {
            problems->push_back(injury->code + ": has no intensity questions");
        }
        for (const Question& q : injury->questions) {
            if (q.options.size() < 2) {
                problems->push_back(injury->code + ": question \"" + q.text + "\" needs at least 2 options");
            }
        }
        if (injury->rules.empty() || injury->rules.front().minPoints != 0) {
            problems->push_back(injury->code + ": needs a severity rule with min_points 0");
        }
    }
    return problems->size() == before;
}

bool Triage::assess(Request& request, std::string* error) const {
    const Injury* injury = catalog.find(request.injuryCode);
    if (injury == nullptr) {
        *error = "Unknown injury type: " + request.injuryCode;
        return false;
    }
    if (request.answers.size() != injury->questions.size()) {
        *error = "Expected " + std::to_string(injury->questions.size()) + " answers for " +
                 injury->name + ", got " + std::to_string(request.answers.size());
        return false;
    }
    if (request.patientCount < 1) {
        *error = "Patient count must be at least 1";
        return false;
    }

    int points = 0;
    for (size_t i = 0; i < injury->questions.size(); ++i) {
        const Question& question = injury->questions[i];
        int choice = request.answers[i];
        if (choice < 0 || choice >= static_cast<int>(question.options.size())) {
            *error = "Invalid answer for question: " + question.text;
            return false;
        }
        points += question.options[choice].points;
    }

    const SeverityRule* rule = injury->ruleFor(points);
    if (rule == nullptr) {
        *error = "No severity rule for " + injury->name + " with " + std::to_string(points) + " points";
        return false;
    }

    int crowdBonus = request.patientCount - 1;
    if (crowdBonus > kMaxCrowdBonus) crowdBonus = kMaxCrowdBonus;

    request.points = points;
    request.priorityScore = injury->basePriority + points + crowdBonus;
    request.severity = rule->severity;
    request.requiredSpecialization = rule->specialization;
    request.requiredFacilities = rule->facilities;
    request.requiredAmbulance = rule->ambulance;
    return true;
}

bool RequestOrder::operator()(const Request* a, const Request* b) const {
    if (a->severity != b->severity) return a->severity > b->severity;
    if (a->priorityScore != b->priorityScore) return a->priorityScore > b->priorityScore;
    if (a->receivedAt != b->receivedAt) return a->receivedAt < b->receivedAt;
    return a->sequence < b->sequence;
}

std::vector<Request*> RequestQueue::ordered() const {
    PriorityQueue<Request*, RequestOrder> copy = heap;  // deep copy, then drain it
    std::vector<Request*> result;
    while (!copy.empty()) result.push_back(copy.pop());
    return result;
}

}  // namespace rahat
