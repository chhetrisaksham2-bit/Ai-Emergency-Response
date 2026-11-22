// RAHAT - Phase 2 demo: distance and travel-time calculation only.
//
// Loads the city map (landmarks, roads, traffic patterns) and uses Dijkstra
// to find, between two landmarks at a chosen time of day:
//   1. the SHORTEST route (fewest km)
//   2. the FASTEST route (least travel time with traffic)
//
// Build:
//   clang++ -std=c++17 -o build/distance_demo apps/distance_demo.cpp modules/routing/routing.cpp modules/models/models.cpp
// Run:
//   ./build/distance_demo                    (asks for places and time)
//   ./build/distance_demo 1 16 18:00         (from landmark 1 to 16 at 18:00)
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../modules/models/models.h"
#include "../modules/routing/routing.h"

using namespace rahat;

namespace {

std::string trim(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// Reads a simple CSV file: skips '#' comments, blank lines and the header row.
bool readRows(const std::string& path, std::vector<std::vector<std::string>>* rows) {
    std::ifstream file(path);
    if (!file) {
        std::cout << "Cannot open " << path << "\n";
        return false;
    }
    std::string line;
    bool headerSkipped = false;
    while (std::getline(file, line)) {
        if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line = line.substr(3);  // Excel BOM
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        std::vector<std::string> fields;
        std::stringstream parts(line);
        std::string field;
        while (std::getline(parts, field, ',')) fields.push_back(trim(field));
        if (!line.empty() && line.back() == ',') fields.push_back("");
        rows->push_back(fields);
    }
    return true;
}

bool loadMap(const std::string& dir, CityMap* map) {
    std::vector<std::vector<std::string>> rows;
    std::string error;

    if (!readRows(dir + "/traffic_patterns.csv", &rows)) return false;
    for (const auto& r : rows) {  // pattern,start,end,level
        int start = 0, end = 0;
        TrafficLevel level;
        if (r.size() < 4 || !parseClock(r[1], &start) || !parseClock(r[2], &end) || !parseTrafficLevel(r[3], &level) ||
            !map->traffic().addSlot(r[0], start, end, level, &error)) {
            std::cout << "Bad traffic pattern row: " << r[0] << " " << error << "\n";
            return false;
        }
    }

    rows.clear();
    if (!readRows(dir + "/landmarks.csv", &rows)) return false;
    for (const auto& r : rows) {  // id,name
        if (r.size() < 2 || !map->addLandmark(std::stoi(r[0]), r[1], &error)) {
            std::cout << "Bad landmark row: " << error << "\n";
            return false;
        }
    }

    rows.clear();
    if (!readRows(dir + "/roads.csv", &rows)) return false;
    for (const auto& r : rows) {  // id,from,to,distance_km,speed_kmph,usual_traffic,pattern
        Road road;
        if (r.size() < 6) {
            std::cout << "Bad road row\n";
            return false;
        }
        road.id = r[0];
        road.distanceKm = std::stod(r[3]);
        road.speedKmph = std::stod(r[4]);
        if (!parseTrafficLevel(r[5], &road.usualLevel)) {
            std::cout << "Bad traffic level on road " << r[0] << "\n";
            return false;
        }
        road.pattern = r.size() > 6 ? r[6] : "";
        if (!map->addRoad(std::stoi(r[1]), std::stoi(r[2]), road, &error)) {
            std::cout << "Bad road " << r[0] << ": " << error << "\n";
            return false;
        }
    }
    return true;
}

int askNumber(const std::string& prompt, int low, int high) {
    while (true) {
        std::cout << prompt << " [" << low << "-" << high << "]: ";
        std::string text;
        if (!std::getline(std::cin, text)) return -1;
        try {
            int value = std::stoi(text);
            if (value >= low && value <= high) return value;
        } catch (...) {
        }
        std::cout << "  Please enter a number from " << low << " to " << high << ".\n";
    }
}

void printRoute(const CityMap& map, const PathTree& tree, int from, int to, const char* title, int minute) {
    std::cout << "\n" << title << "\n";
    if (std::isinf(map.minutesTo(tree, to))) {
        std::cout << "  No road connects these places.\n";
        return;
    }
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Distance    : " << map.kmTo(tree, to) << " km\n";
    std::cout << "  Travel time : " << map.minutesTo(tree, to) << " min (leaving at " << formatClock(minute) << ")\n";
    std::cout << "  Route       :\n";
    DoublyLinkedList<Checkpoint> route = map.routeFromSource(tree, to, minute);
    route.forEach([](const Checkpoint& c) {
        std::cout << "      " << formatClock(c.atMinute) << "  " << c.name << "\n";
    });
    (void)from;
}

}  // namespace

int main(int argc, char** argv) {
    CityMap map;
    if (!loadMap("data", &map)) {
        std::cout << "Run this from the RAHAT folder (it reads data/landmarks.csv, roads.csv, traffic_patterns.csv).\n";
        return 1;
    }
    const std::vector<int>& ids = map.landmarkIds();

    std::cout << "RAHAT - Phase 2: distance and travel time\n";
    std::cout << "Loaded " << map.landmarkCount() << " places and " << map.roadCount() << " roads.\n\n";

    int from = -1, to = -1, minute = -1;
    if (argc == 4) {
        from = std::atoi(argv[1]);
        to = std::atoi(argv[2]);
        if (!parseClock(argv[3], &minute) || !map.hasLandmark(from) || !map.hasLandmark(to)) {
            std::cout << "Usage: distance_demo FROM_ID TO_ID HH:MM\n";
            return 1;
        }
    } else {
        for (size_t i = 0; i < ids.size(); ++i) std::cout << "  " << std::setw(2) << ids[i] << ". " << map.landmarkName(ids[i]) << "\n";
        std::cout << "\n";
        from = askNumber("Start from (landmark number)", ids.front(), ids.back());
        to = askNumber("Go to      (landmark number)", ids.front(), ids.back());
        int hour = askNumber("Hour of the day (24-hour clock)", 0, 23);
        if (from < 0 || to < 0 || hour < 0) return 0;
        minute = hour * 60;
    }

    std::cout << "\nFrom " << map.landmarkName(from) << " to " << map.landmarkName(to) << " at " << formatClock(minute)
              << "\n";

    PathTree shortest = map.shortestFrom(from, minute, RouteMetric::Distance);
    PathTree fastest = map.shortestFrom(from, minute, RouteMetric::TravelTime);

    printRoute(map, shortest, from, to, "1. SHORTEST route (fewest km)", minute);
    printRoute(map, fastest, from, to, "2. FASTEST route (least time, with traffic)", minute);

    std::cout << "\nTravel time on a road = distance / speed x 60 x traffic multiplier\n"
                 "(Clear 1.0, Moderate 1.4, Heavy 2.0, Jam 3.0). Both routes are found with Dijkstra.\n";
    return 0;
}
