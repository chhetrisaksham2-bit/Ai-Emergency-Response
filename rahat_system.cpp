#include "rahat_system.h"

#include <algorithm>

#include "../datastructures/dijkstra.h"

namespace phase2 {

// ---------------------------------------------------------------- building the city

bool RahatSystem::build(const Dataset& dataset, std::string* error) {
    data = &dataset;

    // Every landmark becomes a vertex of the graph.
    for (const Landmark& landmark : dataset.landmarks) {
        vertexOf[landmark.id] = city.addVertex();
        landmarkOf.push_back(landmark.id);
    }
    // Every road becomes a two-way edge, weighted by its length in km.
    for (const Road& road : dataset.roads) {
        auto from = vertexOf.find(road.from), to = vertexOf.find(road.to);
        if (from == vertexOf.end() || to == vertexOf.end() ||
            !city.addUndirectedEdge(from->second, to->second, road.km)) {
            *error = "Road " + road.id + " could not be added to the map";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------- decision 1: severity

bool RahatSystem::assess(const Injury& injury, const std::vector<int>& answers, Assessment* result,
                         std::string* error) const {
    if (answers.size() != injury.questions.size()) {
        *error = "Expected " + std::to_string(injury.questions.size()) + " answers, got " +
                 std::to_string(answers.size());
        return false;
    }
    if (injury.rules.empty()) {
        *error = injury.name + " has no severity rules";
        return false;
    }

    int points = 0;
    for (size_t i = 0; i < answers.size(); ++i) {
        const Question& question = injury.questions[i];
        if (answers[i] < 0 || answers[i] >= static_cast<int>(question.options.size())) {
            *error = "Invalid answer for: " + question.text;
            return false;
        }
        points += question.options[answers[i]].points;
    }

    // Rules are sorted by minPoints, so the last one we pass is the highest that applies.
    const SeverityRule* chosen = &injury.rules.front();
    for (const SeverityRule& rule : injury.rules) {
        if (rule.minPoints <= points) chosen = &rule;
    }

    result->points = points;
    result->severity = chosen->severity;
    result->facilities = chosen->facilities;
    result->ambulance = chosen->ambulance;
    return true;
}

// ---------------------------------------------------------------- decision 2: hospital

std::vector<std::string> RahatSystem::missingFacilities(const Hospital& hospital,
                                                        const std::vector<std::string>& needed) {
    std::vector<std::string> missing;
    for (const std::string& facility : needed) {
        if (std::find(hospital.facilities.begin(), hospital.facilities.end(), facility) == hospital.facilities.end()) {
            missing.push_back(facility);
        }
    }
    return missing;
}

std::vector<HospitalOption> RahatSystem::rankHospitals(int patientLandmark,
                                                       const std::vector<std::string>& neededFacilities) const {
    std::vector<HospitalOption> options;
    auto start = vertexOf.find(patientLandmark);
    if (data == nullptr || start == vertexOf.end()) return options;

    // ONE Dijkstra run from the patient gives the distance to every hospital.
    ShortestPaths fromPatient = dijkstra(city, start->second);

    for (const Hospital& hospital : data->hospitals) {
        int vertex = vertexOf.at(hospital.landmarkId);
        if (!fromPatient.reachable(vertex)) continue;  // no road connects them

        HospitalOption option;
        option.hospital = &hospital;
        option.km = fromPatient.distance[vertex];
        for (int v : fromPatient.pathTo(vertex)) option.route.push_back(landmarkOf[v]);
        option.missing = missingFacilities(hospital, neededFacilities);
        option.canTreat = option.missing.empty();
        options.push_back(option);
    }

    // Nearest first. Equal distances keep the order of hospitals.csv.
    std::stable_sort(options.begin(), options.end(),
                     [](const HospitalOption& a, const HospitalOption& b) { return a.km < b.km; });
    return options;
}

const HospitalOption* RahatSystem::best(const std::vector<HospitalOption>& ranked) {
    for (const HospitalOption& option : ranked) {
        if (option.canTreat) return &option;  // nearest first, so the first that can treat wins
    }
    return nullptr;
}

}  // namespace phase2
