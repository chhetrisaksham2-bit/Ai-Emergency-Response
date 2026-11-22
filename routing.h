// Routing module: the city map, traffic by time of day, and Dijkstra.
//
// Landmarks are graph vertices, roads are two-way edges. A road's travel time is
//   distance / speed * 60 * trafficMultiplier(level at that time of day)
// so the fastest route is not always the shortest one.
//
// Traffic: every road has a usual level (CLEAR / MODERATE / HEAVY / JAM) and may
// follow a named pattern (e.g. MARKET) with time slots stored in a BST keyed by
// slot start minute. Outside every slot, the usual level applies.
//
// Simplification: a trip uses the traffic at the moment the route is planned.
#pragma once

#include <string>
#include <vector>

#include "../ds/bst.h"
#include "../ds/doubly_linked_list.h"
#include "../ds/graph.h"
#include "../ds/hash_map.h"
#include "../models/models.h"

namespace rahat {

enum class TrafficLevel { Clear, Moderate, Heavy, Jam };

std::string toString(TrafficLevel level);
bool parseTrafficLevel(const std::string& text, TrafficLevel* out);
double trafficMultiplier(TrafficLevel level);

struct Road {
    std::string id;
    double distanceKm = 0;
    double speedKmph = 0;  // speed limit / normal speed on a clear road
    TrafficLevel usualLevel = TrafficLevel::Clear;
    std::string pattern;   // empty = no time pattern
};

class TrafficPatterns {
public:
    // Adds a slot [start, end) in minutes of day. A slot that crosses midnight
    // (e.g. 22:00-06:00) is stored as two slots. Overlapping slots are rejected.
    bool addSlot(const std::string& pattern, int start, int end, TrafficLevel level, std::string* error);

    // Level for a road following `pattern` at this minute; `usual` if no slot covers it.
    TrafficLevel levelAt(const std::string& pattern, int minuteOfDay, TrafficLevel usual) const;

    bool hasPattern(const std::string& pattern) const { return patterns.contains(pattern); }

private:
    struct Slot {
        int end;
        TrafficLevel level;
    };
    HashMap<std::string, BST<int, Slot>> patterns;

    bool insertSlot(const std::string& pattern, int start, int end, TrafficLevel level, std::string* error);
};

enum class RouteMetric { TravelTime, Distance };

// Result of one Dijkstra run, indexed by internal vertex number.
struct PathTree {
    int source = -1;
    int minuteOfDay = 0;
    std::vector<double> minutes;  // travel time from the source along the chosen path
    std::vector<double> km;       // distance from the source along the chosen path
    std::vector<int> parent;      // previous vertex on the path, -1 for the source
};

class CityMap {
public:
    bool addLandmark(int id, const std::string& name, std::string* error);
    bool addRoad(int fromLandmark, int toLandmark, const Road& road, std::string* error);

    bool hasLandmark(int id) const { return indexById.contains(id); }
    std::string landmarkName(int id) const;
    const std::vector<int>& landmarkIds() const { return idByIndex; }  // in the order added
    int landmarkCount() const { return static_cast<int>(idByIndex.size()); }
    int roadCount() const { return roads; }

    TrafficPatterns& traffic() { return patterns; }
    const TrafficPatterns& traffic() const { return patterns; }

    TrafficLevel levelOn(const Road& road, int minuteOfDay) const;
    double travelMinutes(const Road& road, int minuteOfDay) const;

    // Dijkstra from a landmark. Default: fastest travel time at that time of day.
    PathTree shortestFrom(int landmarkId, int minuteOfDay, RouteMetric metric = RouteMetric::TravelTime) const;

    double minutesTo(const PathTree& tree, int landmarkId) const;  // infinity if unreachable
    double kmTo(const PathTree& tree, int landmarkId) const;

    // Roads are two-way with the same traffic both ways, so one tree from the
    // patient gives both legs of an ambulance trip:
    //   routeToSource:   hospital -> ... -> patient
    //   routeFromSource: patient  -> ... -> hospital
    // Checkpoint times are startMinute + travel time to reach that checkpoint.
    DoublyLinkedList<Checkpoint> routeToSource(const PathTree& tree, int fromLandmark, double startMinute) const;
    DoublyLinkedList<Checkpoint> routeFromSource(const PathTree& tree, int toLandmark, double startMinute) const;

private:
    Graph<Road> graph;
    HashMap<int, int> indexById;  // landmark id -> vertex number
    std::vector<int> idByIndex;   // vertex number -> landmark id
    std::vector<std::string> names;
    TrafficPatterns patterns;
    int roads = 0;

    int indexOf(int landmarkId) const;
};

}  // namespace rahat
