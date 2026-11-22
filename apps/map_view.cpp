#include "map_view.h"

#include <cstdio>
#include <cstdlib>

#include <ftxui/dom/canvas.hpp>

#include "../modules/data_io/data_io.h"

using namespace ftxui;

namespace rahat {

namespace {

bool toNumber(const std::string& text, double* out) {
    char* end = nullptr;
    *out = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0';
}

std::pair<int, int> edge(int a, int b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); }

}  // namespace

int CityPicture::landmarkId(const std::string& name) const {
    auto it = idByName.find(name);
    return it == idByName.end() ? -1 : it->second;
}

bool loadCityPicture(const std::string& dataDir, CityPicture* picture, std::string* error) {
    *picture = CityPicture();
    CsvTable landmarks, roads, hospitals, layout;
    if (!readCsv(dataDir + "/landmarks.csv", {"id", "name"}, &landmarks, error) ||
        !readCsv(dataDir + "/roads.csv", {"from", "to", "distance_km"}, &roads, error) ||
        !readCsv(dataDir + "/hospitals.csv", {"name", "landmark"}, &hospitals, error) ||
        !readCsv(dataDir + "/map_layout.csv", {"id", "x", "y"}, &layout, error)) {
        return false;
    }
    double number = 0;
    for (size_t r = 0; r < landmarks.rows.size(); ++r) {
        if (toNumber(landmarks.value(r, landmarks.column("id")), &number)) {
            picture->idByName[landmarks.value(r, landmarks.column("name"))] = static_cast<int>(number);
        }
    }
    for (size_t r = 0; r < roads.rows.size(); ++r) {
        double from = 0, to = 0, km = 0;
        if (toNumber(roads.value(r, roads.column("from")), &from) && toNumber(roads.value(r, roads.column("to")), &to) &&
            toNumber(roads.value(r, roads.column("distance_km")), &km)) {
            picture->roads.push_back({static_cast<int>(from), static_cast<int>(to), km});
        }
    }
    for (size_t r = 0; r < hospitals.rows.size(); ++r) {
        if (toNumber(hospitals.value(r, hospitals.column("landmark")), &number)) {
            picture->hospitalAt.insert(static_cast<int>(number));
            picture->hospitalLandmark[hospitals.value(r, hospitals.column("name"))] = static_cast<int>(number);
        }
    }
    for (size_t r = 0; r < layout.rows.size(); ++r) {
        double id = 0, x = 0, y = 0;
        if (!toNumber(layout.value(r, layout.column("id")), &id) || !toNumber(layout.value(r, layout.column("x")), &x) ||
            !toNumber(layout.value(r, layout.column("y")), &y) || x < 0 || x > 100 || y < 0 || y > 100) {
            *error = layout.where(r) + ": needs id, and x and y from 0 to 100";
            return false;
        }
        picture->position[static_cast<int>(id)] = {x, y};
    }
    return true;
}

std::vector<int> routeFromLines(const CityPicture& picture, const std::vector<std::string>& lines) {
    std::vector<int> route;
    for (std::string line : lines) {
        if (line.size() > 7 && line[2] == ':') line = line.substr(7);  // drop "HH:MM  "
        size_t marker = line.find("  <-");
        if (marker != std::string::npos) line = line.substr(0, marker);
        int id = picture.landmarkId(line);
        if (id >= 0 && (route.empty() || route.back() != id)) route.push_back(id);
    }
    return route;
}

Element drawMap(const CityPicture& picture, const MapMarks& marks, int columns, int rows) {
    const int width = columns * 2;  // a character is 2 x 4 canvas pixels
    const int height = rows * 4;

    std::set<std::pair<int, int>> routeEdges;
    std::set<int> routeNodes(marks.route.begin(), marks.route.end());
    for (size_t i = 1; i < marks.route.size(); ++i) routeEdges.insert(edge(marks.route[i - 1], marks.route[i]));

    // The canvas draws later, when the screen is rendered, so everything it uses is copied.
    std::map<int, std::pair<double, double>> position = picture.position;
    auto place = [position, width, height](int id, int* px, int* py) {
        auto it = position.find(id);
        if (it == position.end()) return false;
        *px = 6 + static_cast<int>(it->second.first / 100.0 * (width - 16));
        *py = 6 + static_cast<int>(it->second.second / 100.0 * (height - 14));
        return true;
    };
    std::vector<CityPicture::Line> roads = picture.roads;
    std::set<int> hospitalAt = picture.hospitalAt;

    Element image = canvas(width, height, [roads, position, hospitalAt, marks, routeEdges, routeNodes, place](Canvas& c) {
        for (const CityPicture::Line& road : roads) {  // every road, thin grey
            int x1, y1, x2, y2;
            if (!place(road.from, &x1, &y1) || !place(road.to, &x2, &y2)) continue;
            if (!routeEdges.count(edge(road.from, road.to))) c.DrawPointLine(x1, y1, x2, y2, Color::GrayDark);
        }
        for (const CityPicture::Line& road : roads) {  // the route, thick green, with its distances
            int x1, y1, x2, y2;
            if (!place(road.from, &x1, &y1) || !place(road.to, &x2, &y2)) continue;
            if (!routeEdges.count(edge(road.from, road.to))) continue;
            c.DrawPointLine(x1, y1, x2, y2, Color::Green);
            c.DrawPointLine(x1 + 1, y1, x2 + 1, y2, Color::Green);
            c.DrawPointLine(x1, y1 + 1, x2, y2 + 1, Color::Green);
            char km[16];
            std::snprintf(km, sizeof km, "%.1f", road.km);
            c.DrawText((x1 + x2) / 2 - 4, (y1 + y2) / 2 - 6, km, Color::GreenLight);
        }
        for (const auto& item : position) {  // landmarks: same numbers as in the patient's list
            int id = item.first, x, y;
            if (!place(id, &x, &y)) continue;
            bool hospital = hospitalAt.count(id) > 0;
            Color color = Color::White;
            if (routeNodes.count(id)) color = Color::GreenLight;
            if (hospital) color = id == marks.chosenHospitalLandmark ? Color::Green : Color::Red;
            if (id == marks.patientLandmark) {
                c.DrawPointCircle(x, y, 7, Color::Yellow);
                c.DrawPointCircle(x, y, 6, Color::Yellow);
            }
            c.DrawPointCircle(x, y, 4, color);
            std::string label = (hospital ? "+" : "") + std::to_string(id);
            c.DrawText(x - static_cast<int>(label.size()), y - 2, label, color);
        }
    });

    Element legend = hbox({text("+n") | color(Color::Red), text(" hospital  "), text("o") | color(Color::Yellow),
                           text(" patient  "), text("--") | color(Color::Green), text(" ambulance route")}) |
                     dim;
    return vbox({image, legend});
}

}  // namespace rahat
