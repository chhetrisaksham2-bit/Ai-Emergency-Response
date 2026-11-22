// Triage module: turns the patient's answers into a priority and decides what
// care the patient needs. Also keeps waiting requests in priority order.
//
//   priorityScore = injury base priority
//                 + points from intensity answers
//                 + crowd bonus (1 per extra patient, max 5)
//
//   severity = the highest severity rule whose minPoints <= points.
//   That rule also says which specialist, facilities and ambulance type are needed.
#pragma once

#include <string>
#include <vector>

#include "../ds/hash_map.h"
#include "../ds/priority_queue.h"
#include "../models/models.h"

namespace rahat {

struct AnswerOption {
    std::string text;
    int points = 0;
};

struct Question {
    std::string text;
    std::vector<AnswerOption> options;
};

struct SeverityRule {
    Severity severity = Severity::Minor;
    int minPoints = 0;
    std::string specialization;
    std::vector<std::string> facilities;
    AmbulanceType ambulance = AmbulanceType::BLS;
};

struct Injury {
    std::string code;  // e.g. SNAKE_BITE
    std::string name;  // e.g. Snake bite
    int basePriority = 0;
    std::vector<Question> questions;
    std::vector<SeverityRule> rules;  // kept sorted by minPoints

    void addRule(const SeverityRule& rule);
    const SeverityRule* ruleFor(int points) const;
};

class InjuryCatalog {
public:
    InjuryCatalog() = default;
    ~InjuryCatalog();
    InjuryCatalog(const InjuryCatalog&) = delete;  // owns Injury objects; copying is blocked
    InjuryCatalog& operator=(const InjuryCatalog&) = delete;

    // Returns nullptr if the code is already used.
    Injury* addInjury(const std::string& code, const std::string& name, int basePriority);
    Injury* find(const std::string& code);
    const Injury* find(const std::string& code) const;
    const std::vector<Injury*>& all() const { return ordered; }  // menu order

    // Checks every injury has questions with options and a rule starting at 0 points.
    bool validate(std::vector<std::string>* problems) const;

private:
    HashMap<std::string, Injury*> byCode;
    std::vector<Injury*> ordered;
};

const int kMaxCrowdBonus = 5;

class Triage {
public:
    explicit Triage(const InjuryCatalog& catalog) : catalog(catalog) {}

    // Validates the request's answers and fills points, priorityScore,
    // severity and the care requirements. Returns false with a reason if invalid.
    bool assess(Request& request, std::string* error) const;

private:
    const InjuryCatalog& catalog;
};

// Order for the request queue: higher severity first, then higher score,
// then whoever asked first.
struct RequestOrder {
    bool operator()(const Request* a, const Request* b) const;
};

class RequestQueue {
public:
    void push(Request* request) { heap.push(request); }
    Request* pop() { return heap.pop(); }
    bool empty() const { return heap.empty(); }
    int size() const { return heap.size(); }

    // All waiting requests in the order they will be served (queue unchanged).
    std::vector<Request*> ordered() const;

private:
    PriorityQueue<Request*, RequestOrder> heap;
};

}  // namespace rahat
