// RahatSystem - the decision-making engine of Phase 2.
// Owner: Saksham Chhetri (System Development)
//
// It takes the records from the DATASET part and the GRAPH / DIJKSTRA from the
// DATA STRUCTURES part, and makes the two decisions RAHAT exists for:
//
//   1. assess()        How serious is the patient?
//                        points   = sum of the points of the chosen answers
//                        severity = rule with the highest min_points <= points
//                        the rule also says which facilities and ambulance are needed
//
//   2. rankHospitals() Where should the patient go?
//                        one Dijkstra run from the patient's landmark
//                        -> distance and route to every hospital, nearest first
//      best()          -> the NEAREST hospital that has EVERY facility needed
#pragma once

#include <map>
#include <string>
#include <vector>

#include "../dataset/records.h"
#include "../datastructures/graph.h"

namespace phase2 {

struct Assessment {
    int points = 0;
    Severity severity = Severity::Minor;
    std::vector<std::string> facilities;  // what the hospital must have
    std::string ambulance;                // BLS or ALS
};

struct HospitalOption {
    const Hospital* hospital = nullptr;
    double km = 0;
    std::vector<int> route;            // landmark ids: patient's landmark ... hospital's landmark
    bool canTreat = false;
    std::vector<std::string> missing;  // facilities this hospital does not have
};

class RahatSystem {
public:
    // Builds the city graph from the dataset. The dataset must stay alive while the system is used.
    bool build(const Dataset& data, std::string* error);

    bool assess(const Injury& injury, const std::vector<int>& answers, Assessment* result,
                std::string* error) const;

    // Every hospital the patient can reach by road, nearest first.
    std::vector<HospitalOption> rankHospitals(int patientLandmark,
                                              const std::vector<std::string>& neededFacilities) const;

    // The nearest hospital that can treat the patient, or nullptr if none can.
    static const HospitalOption* best(const std::vector<HospitalOption>& ranked);

    const Dataset& dataset() const { return *data; }

private:
    const Dataset* data = nullptr;
    Graph city;                       // vertex = landmark, edge = road (weight = km)
    std::map<int, int> vertexOf;      // landmark id -> graph vertex number
    std::vector<int> landmarkOf;      // graph vertex number -> landmark id

    static std::vector<std::string> missingFacilities(const Hospital& hospital,
                                                      const std::vector<std::string>& needed);
};

}  // namespace phase2
