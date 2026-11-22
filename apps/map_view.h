// Terminal map for the patient and hospital windows (FTXUI canvas), from Phase 2.
//
// Landmarks are drawn at the positions in data/map_layout.csv, roads are the
// lines from roads.csv, hospitals are marked with a red "+", the patient with a
// yellow ring and the ambulance route in green. The positions are only for
// drawing - distances and travel times still come from roads.csv and traffic.
// Without map_layout.csv the windows simply work without the map.
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/dom/elements.hpp>

namespace rahat {

struct CityPicture {
    struct Line {
        int from;
        int to;
        double km;
    };
    std::map<int, std::pair<double, double>> position;  // landmark id -> (x, y), 0..100
    std::map<std::string, int> idByName;
    std::map<std::string, int> hospitalLandmark;        // hospital name -> landmark id
    std::set<int> hospitalAt;
    std::vector<Line> roads;

    bool empty() const { return position.empty(); }
    int landmarkId(const std::string& name) const;  // -1 if unknown
};

// Reads landmarks, roads, hospitals and map_layout from the data folder.
bool loadCityPicture(const std::string& dataDir, CityPicture* picture, std::string* error);

struct MapMarks {
    int patientLandmark = -1;          // yellow ring
    std::vector<int> route;            // landmark ids in order, drawn in green
    int chosenHospitalLandmark = -1;   // drawn green instead of red
};

// Route lines such as "10:09  Clock Tower  <- patient pickup" -> landmark ids.
std::vector<int> routeFromLines(const CityPicture& picture, const std::vector<std::string>& lines);

// A map `columns` x `rows` characters big, plus a one-line legend.
ftxui::Element drawMap(const CityPicture& picture, const MapMarks& marks, int columns, int rows);

}  // namespace rahat
