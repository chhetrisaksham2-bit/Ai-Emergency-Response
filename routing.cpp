#include "routing.h"

#include <limits>

#include "../ds/priority_queue.h"

namespace rahat {

namespace {

const double kInfinity = std::numeric_limits<double>::infinity();

struct QueueItem {
    double key;
    int vertex;
};

struct SmallestKeyFirst {
    bool operator()(const QueueItem& a, const QueueItem& b) const { return a.key < b.key; }
};

}  // namespace

std::string toString(TrafficLevel level) {
    switch (level) {
        case TrafficLevel::Clear: return "CLEAR";
        case TrafficLevel::Moderate: return "MODERATE";
        case TrafficLevel::Heavy: return "HEAVY";
        case TrafficLevel::Jam: return "JAM";
    }
    return "UNKNOWN";
}

bool parseTrafficLevel(const std::string& text, TrafficLevel* out) {
    if (text == "CLEAR") *out = TrafficLevel::Clear;
    else if (text == "MODERATE") *out = TrafficLevel::Moderate;
    else if (text == "HEAVY") *out = TrafficLevel::Heavy;
    else if (text == "JAM") *out = TrafficLevel::Jam;
    else return false;
    return true;
}

double trafficMultiplier(TrafficLevel level) {
    switch (level) {
        case TrafficLevel::Clear: return 1.0;
        case TrafficLevel::Moderate: return 1.4;
        case TrafficLevel::Heavy: return 2.0;
        case TrafficLevel::Jam: return 3.0;
    }
    return 1.0;
}

// ------------------------------------------------------------ TrafficPatterns

bool TrafficPatterns::addSlot(const std::string& pattern, int start, int end, TrafficLevel level,
                              std::string* error) {
    if (start < 0 || start > 1440 || end < 0 || end > 1440 || start == end) {
        *error = "Invalid time slot for pattern " + pattern;
        return false;
    }
    if (start < end) return insertSlot(pattern, start, end, level, error);

    // Crosses midnight: split into [start, 24:00) and [00:00, end).
    if (!insertSlot(pattern, start, 1440, level, error)) return false;
    if (end == 0) return true;
    return insertSlot(pattern, 0, end, level, error);
}

bool TrafficPatterns::insertSlot(const std::string& pattern, int start, int end, TrafficLevel level,
                                 std::string* error) {
    BST<int, Slot>* slots = patterns.find(pattern);
    if (slots == nullptr) {
        patterns.put(pattern, BST<int, Slot>());
        slots = patterns.find(pattern);
    }

    bool overlap = false;
    slots->inOrder([&](const int& slotStart, const Slot& slot) {
        if (start < slot.end && slotStart < end) overlap = true;
    });
    if (overlap) {
        *error = "Time slot " + formatClock(start) + "-" + formatClock(end) + " overlaps another slot in pattern " +
                 pattern;
        return false;
    }
    slots->insert(start, Slot{end, level});
    return true;
}

TrafficLevel TrafficPatterns::levelAt(const std::string& pattern, int minute, TrafficLevel usual) const {
    if (pattern.empty()) return usual;
    const BST<int, Slot>* slots = patterns.find(pattern);
    if (slots == nullptr) return usual;

    // The slot that started most recently at or before this minute...
    const Slot* slot = slots->floor(minute);
    // ...applies only if it has not ended yet.
    if (slot != nullptr && minute < slot->end) return slot->level;
    return usual;
}

// ------------------------------------------------------------ CityMap

bool CityMap::addLandmark(int id, const std::string& name, std::string* error) {
    if (indexById.contains(id)) {
        *error = "Duplicate landmark id " + std::to_string(id);
        return false;
    }
    int vertex = graph.addVertex();
    indexById.put(id, vertex);
    idByIndex.push_back(id);
    names.push_back(name);
    return true;
}

bool CityMap::addRoad(int fromLandmark, int toLandmark, const Road& road, std::string* error) {
    if (!hasLandmark(fromLandmark) || !hasLandmark(toLandmark)) {
        *error = "Road " + road.id + " uses an unknown landmark";
        return false;
    }
    if (fromLandmark == toLandmark) {
        *error = "Road " + road.id + " starts and ends at the same landmark";
        return false;
    }
    if (road.distanceKm <= 0 || road.speedKmph <= 0) {
        *error = "Road " + road.id + " needs a positive distance and speed";
        return false;
    }
    graph.addUndirectedEdge(indexOf(fromLandmark), indexOf(toLandmark), road);
    ++roads;
    return true;
}

std::string CityMap::landmarkName(int id) const {
    const int* index = indexById.find(id);
    return index == nullptr ? "Unknown landmark" : names[*index];
}

int CityMap::indexOf(int landmarkId) const {
    const int* index = indexById.find(landmarkId);
    return index == nullptr ? -1 : *index;
}

TrafficLevel CityMap::levelOn(const Road& road, int minuteOfDay) const {
    return patterns.levelAt(road.pattern, minuteOfDay, road.usualLevel);
}

double CityMap::travelMinutes(const Road& road, int minuteOfDay) const {
    return road.distanceKm / road.speedKmph * 60.0 * trafficMultiplier(levelOn(road, minuteOfDay));
}

PathTree CityMap::shortestFrom(int landmarkId, int minuteOfDay, RouteMetric metric) const {
    PathTree tree;
    int n = graph.vertexCount();
    tree.source = indexOf(landmarkId);
    tree.minuteOfDay = minuteOfDay;
    tree.minutes.assign(n, kInfinity);
    tree.km.assign(n, kInfinity);
    tree.parent.assign(n, -1);
    if (tree.source < 0) return tree;

    // best[v] is the value being minimised (minutes or km, depending on metric).
    std::vector<double> best(n, kInfinity);
    std::vector<bool> done(n, false);
    PriorityQueue<QueueItem, SmallestKeyFirst> queue;

    best[tree.source] = 0;
    tree.minutes[tree.source] = 0;
    tree.km[tree.source] = 0;
    queue.push({0, tree.source});

    while (!queue.empty()) {
        QueueItem item = queue.pop();
        int u = item.vertex;
        if (done[u]) continue;  // an older, longer entry for u (lazy deletion)
        done[u] = true;

        for (Graph<Road>::Edge* edge = graph.edgesOf(u); edge != nullptr; edge = edge->next) {
            int v = edge->to;
            double edgeMinutes = travelMinutes(edge->data, minuteOfDay);
            double edgeKm = edge->data.distanceKm;
            double candidate = best[u] + (metric == RouteMetric::TravelTime ? edgeMinutes : edgeKm);
            if (candidate < best[v]) {
                best[v] = candidate;
                tree.minutes[v] = tree.minutes[u] + edgeMinutes;
                tree.km[v] = tree.km[u] + edgeKm;
                tree.parent[v] = u;
                queue.push({candidate, v});
            }
        }
    }
    return tree;
}

double CityMap::minutesTo(const PathTree& tree, int landmarkId) const {
    int index = indexOf(landmarkId);
    return index < 0 || index >= static_cast<int>(tree.minutes.size()) ? kInfinity : tree.minutes[index];
}

double CityMap::kmTo(const PathTree& tree, int landmarkId) const {
    int index = indexOf(landmarkId);
    return index < 0 || index >= static_cast<int>(tree.km.size()) ? kInfinity : tree.km[index];
}

DoublyLinkedList<Checkpoint> CityMap::routeToSource(const PathTree& tree, int fromLandmark,
                                                    double startMinute) const {
    DoublyLinkedList<Checkpoint> route;
    int v = indexOf(fromLandmark);
    if (v < 0 || v >= static_cast<int>(tree.minutes.size()) || tree.minutes[v] == kInfinity) return route;

    double total = tree.minutes[v];
    // Following parents from v walks v -> ... -> source, which is the trip order.
    for (; v != -1; v = tree.parent[v]) {
        route.pushBack({idByIndex[v], names[v], startMinute + (total - tree.minutes[v])});
    }
    return route;
}

DoublyLinkedList<Checkpoint> CityMap::routeFromSource(const PathTree& tree, int toLandmark,
                                                      double startMinute) const {
    DoublyLinkedList<Checkpoint> route;
    int v = indexOf(toLandmark);
    if (v < 0 || v >= static_cast<int>(tree.minutes.size()) || tree.minutes[v] == kInfinity) return route;

    // Following parents walks backwards, so add each checkpoint at the front.
    for (; v != -1; v = tree.parent[v]) {
        route.pushFront({idByIndex[v], names[v], startMinute + tree.minutes[v]});
    }
    return route;
}

}  // namespace rahat
