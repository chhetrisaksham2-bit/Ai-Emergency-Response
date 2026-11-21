// RAHAT hospital window - one per hospital.
//
// Shows only the patients the system assigned to THIS hospital: the pickup
// point (the patient's landmark), what the ambulance crew needs, the first-aid
// steps and the trip progress. It has one button: "ambulance is back".
// There is no accept / reject: the system has already dispatched the ambulance.
//
//   ./build/hospital --id H1 [--shared DIR] [--data DIR]
#include <filesystem>
#include <iostream>
#include <mutex>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include "hospital_client.h"
#include "map_view.h"
#include "protocol.h"
#include "ui_common.h"

using namespace rahat;
using namespace ftxui;

namespace {

void printUsage(const std::string& sharedDir) {
    std::cout << "Usage: hospital --id HOSPITAL_ID [--shared DIR] [--data DIR]\n"
                 "  --id ID        which hospital this window is, for example H1\n"
                 "  --shared DIR   folder shared with the admin window (default: shared)\n"
                 "  --data DIR     data folder, only used to draw the map (default: data)\n";
    std::error_code code;
    std::filesystem::path folder = std::filesystem::path(sharedDir) / "hospitals";
    std::string ids;
    for (std::filesystem::directory_iterator it(folder, code), end; !code && it != end; it.increment(code)) {
        if (it->is_directory()) ids += " " + it->path().filename().string();
    }
    if (!ids.empty()) std::cout << "Hospitals in this session:" << ids << "\n";
}

std::string stageText(const HospitalAssignment& a) {
    if (a.returned) return "BACK, AVAILABLE";
    if (a.stage == "DISPATCHED") return "GOING TO PICKUP";
    if (a.stage == "ARRIVED_AT_SCENE") return "AT PICKUP";
    if (a.stage == "LEFT_SCENE") return "BRINGING PATIENT";
    if (a.stage == "ARRIVED_AT_HOSPITAL") return "PATIENT DELIVERED";
    return a.stage;
}

bool canReturn(const HospitalAssignment& a) {
    return !a.returned && (a.stage == "LEFT_SCENE" || a.stage == "ARRIVED_AT_HOSPITAL");
}

Element assignmentList(const std::vector<HospitalAssignment>& list, int selected) {
    Elements rows;
    for (size_t i = 0; i < list.size(); ++i) {
        const HospitalAssignment& a = list[i];
        Element row = hbox({
            ui::cell(static_cast<int>(i) == selected ? ">" : "", 2),
            ui::cell(protocol::shortId(a.requestId), 10),
            ui::cell(a.severity, 10) | color(ui::severityColor(a.severity)),
            ui::cell(a.ambulanceId, 5),
            ui::cell("Pickup: " + a.pickup, 26),
            text(stageText(a)) | color(a.returned ? Color::Green : Color::Cyan) | flex,
        });
        rows.push_back(static_cast<int>(i) == selected ? row | inverted : row);
    }
    if (list.empty()) rows.push_back(text("  No patients assigned to this hospital yet.") | dim);
    return vbox(std::move(rows));
}

Element details(const HospitalAssignment& a) {
    Elements rows;
    rows.push_back(hbox({text("PICKUP POINT: ") | bold, text(a.pickup) | bold | color(Color::Yellow)}));
    rows.push_back(hbox({text("Patient:      "), ui::severityLabel(a.severity),
                         text(" (" + std::to_string(a.points) + " points)")}));
    rows.push_back(paragraph("              " + a.injury +
                             (a.patients > 1 ? ", " + std::to_string(a.patients) + " patients at this place" : "")));
    rows.push_back(paragraph("Ambulance:    " + a.ambulanceId + " (" + a.ambulanceType + "), dispatched " + a.dispatchedAt));
    rows.push_back(paragraph("Timing:       reaches the patient about " + a.etaScene + ", back here about " + a.etaHospital));
    rows.push_back(paragraph("Destination:  " + a.destination + " (this hospital)"));
    rows.push_back(paragraph("Doctor ready: " + a.doctor));
    rows.push_back(separator());
    rows.push_back(text("First aid for the crew") | bold);
    for (size_t i = 0; i < a.firstAid.size(); ++i) rows.push_back(paragraph(std::to_string(i + 1) + ". " + a.firstAid[i]));
    rows.push_back(separator());
    rows.push_back(text("Route") | bold);
    for (const std::string& line : a.route) rows.push_back(text(line) | dim);
    rows.push_back(separator());
    rows.push_back(text("Progress") | bold);
    for (const std::string& line : a.timeline) rows.push_back(paragraph(line));
    return vbox(std::move(rows));
}

Element fleetPanel(const std::vector<HospitalAmbulance>& fleet) {
    Elements rows;
    for (const HospitalAmbulance& a : fleet) {
        Color c = a.status == "AVAILABLE" ? Color::Green : a.status == "AT_HOSPITAL" ? Color::Yellow : Color::Cyan;
        rows.push_back(hbox({ui::cell(a.id, 5), ui::cell(a.type, 5), ui::cell(a.status, 13) | color(c),
                             text(a.requestId.empty() ? "" : protocol::shortId(a.requestId)) | dim}));
    }
    if (fleet.empty()) rows.push_back(text("-") | dim);
    return vbox(std::move(rows));
}

}  // namespace

int main(int argc, char** argv) {
    ui::Options options(argc, argv);
    std::string sharedDir = options.get("--shared", "shared");
    std::string id = options.get("--id", "");
    if (options.has("--help") || id.empty() || !options.problems().empty()) {
        for (const std::string& p : options.problems()) std::cout << p << "\n";
        printUsage(sharedDir);
        return id.empty() && !options.has("--help") ? 1 : 0;
    }

    HospitalClient client(sharedDir, id);
    CityPicture picture;
    std::string mapError;
    bool hasMap = loadCityPicture(options.get("--data", "data"), &picture, &mapError);  // optional
    std::mutex lock;
    int selected = 0;

    ScreenInteractive screen = ScreenInteractive::Fullscreen();
    ui::Poller poller(screen, 500, [&] {
        std::lock_guard<std::mutex> guard(lock);
        return client.poll();
    });

    Component root = Renderer([&] {
        std::lock_guard<std::mutex> guard(lock);
        const std::vector<HospitalAssignment>& list = client.assignments();
        if (selected >= static_cast<int>(list.size())) selected = static_cast<int>(list.size()) - 1;
        if (selected < 0) selected = 0;

        int free = client.freeAmbulances();
        int total = static_cast<int>(client.ambulances().size());
        int mapColumns = 0, mapRows = 0;
        ui::mapSize(70, &mapColumns, &mapRows);  // left panel is 72 wide

        Element header = hbox({
            text(" RAHAT ") | bold | color(Color::Red),
            text("Hospital window - " + client.name() + " (" + client.id() + ")") | bold,
            filler(),
            text("time " + client.clock() + "   "),
            text("Ambulances available: " + std::to_string(free) + " of " + std::to_string(total) + " ") | bold |
                color(free == 0 ? Color::Red : Color::Green),
        });

        if (!client.connected()) {
            return vbox({header, filler(),
                         text("Waiting for the admin window. Start it first: ./build/admin") | center | color(Color::Yellow),
                         filler()});
        }

        Element body = text("Select a patient on the left.") | dim;
        Element button = text("");
        MapMarks marks;
        auto here = picture.hospitalLandmark.find(client.name());
        if (here != picture.hospitalLandmark.end()) marks.chosenHospitalLandmark = here->second;
        if (!list.empty()) {
            const HospitalAssignment& a = list[selected];
            body = details(a);
            marks.patientLandmark = picture.landmarkId(a.pickup);
            if (!a.returned) marks.route = routeFromLines(picture, a.route);
            bool enabled = canReturn(a);
            button = text(a.returned ? " Ambulance " + a.ambulanceId + " is back " : " [r] Ambulance " + a.ambulanceId + " is back ") |
                     border | color(enabled ? Color::Green : Color::GrayDark);
        }
        Element result = client.lastResult().empty()
                             ? text("")
                             : text(" " + client.lastResult()) | color(client.lastResultOk() ? Color::Green : Color::Red);

        return vbox({
            header,
            hbox({vbox({ui::titled("Patients assigned to this hospital", assignmentList(list, selected)) | flex,
                        ui::titled("Our ambulances", fleetPanel(client.ambulances())),
                        hasMap ? ui::titled("Map - pickup point and route", drawMap(picture, marks, mapColumns, mapRows))
                               : text("")}) |
                      size(WIDTH, EQUAL, 72),
                  ui::titled("Patient details", body | vscroll_indicator | yframe) | flex}) |
                flex,
            hbox({button, result | vcenter}),
            text(" Up/Down: choose a patient    r: ambulance is back (return button)    q: quit") | dim,
        });
    });

    root = CatchEvent(root, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        std::lock_guard<std::mutex> guard(lock);
        const std::vector<HospitalAssignment>& list = client.assignments();
        if (event == Event::ArrowDown) {
            if (selected + 1 < static_cast<int>(list.size())) ++selected;
            return true;
        }
        if (event == Event::ArrowUp) {
            if (selected > 0) --selected;
            return true;
        }
        if ((event == Event::Character('r') || event == Event::Character('R')) && !list.empty()) {
            std::string error;
            client.pressReturned(list[selected].ambulanceId, &error);  // result line shows the outcome
            return true;
        }
        return event == Event::Custom;
    });

    screen.Loop(root);
    return 0;
}
