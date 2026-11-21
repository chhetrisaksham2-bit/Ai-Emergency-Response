// RAHAT admin / server window.
//
// The central program: receives every request, runs triage, keeps the priority
// queue, checks every hospital, assigns the hospital and ambulance, and sends
// each window only what it needs. Start it FIRST - it starts a new session and
// clears the shared folder.
//
//   ./build/admin [--time HH:MM] [--speed N] [--batch MINUTES] [--timeout MINUTES]
//                 [--data DIR] [--shared DIR]
#include <iostream>
#include <mutex>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "admin_server.h"
#include "protocol.h"
#include "ui_common.h"

using namespace rahat;
using namespace ftxui;

namespace {

void printUsage() {
    std::cout << "Usage: admin [options]\n"
                 "  --time HH:MM       start the simulated clock at this time (default: now)\n"
                 "  --speed N          simulated seconds per real second (default: 60)\n"
                 "  --batch MINUTES    collect new requests this long, then serve them by priority\n"
                 "                     (simulated minutes, default 1; CRITICAL never waits)\n"
                 "  --timeout MINUTES  count an ambulance back this long after dispatch if the hospital\n"
                 "                     never presses its return button (default 75 = 1 h 15 min)\n"
                 "  --data DIR         folder with the CSV files (default: data)\n"
                 "  --shared DIR       folder shared with the other windows (default: shared)\n";
}

Element requestTable(const AdminView& view, int selected) {
    Elements rows;
    rows.push_back(hbox({ui::cell("", 2), ui::cell("Request", 10), ui::cell("Severity", 10), ui::cell("Score", 6),
                         ui::cell("Injury", 26), ui::cell("Place", 17), ui::cell("Status", 12), text("Hospital")}) |
                   dim);
    for (size_t i = 0; i < view.active.size(); ++i) {
        const AdminView::RequestRow& r = view.active[i];
        std::string injury = r.injury + (r.patients > 1 ? " x" + std::to_string(r.patients) : "");
        Element row = hbox({
            ui::cell(static_cast<int>(i) == selected ? ">" : "", 2),
            ui::cell(protocol::shortId(r.id), 10),
            ui::cell(r.severity, 10) | color(ui::severityColor(r.severity)),
            ui::cell(std::to_string(r.priorityScore), 6),
            ui::cell(injury, 26),
            ui::cell(r.landmark, 17),
            ui::cell(r.status, 12) | color(r.status == "WAITING" ? Color::Yellow : Color(Color::Default)),
            text(r.hospitals.empty() ? "-" : r.hospitals) | flex,
        });
        rows.push_back(static_cast<int>(i) == selected ? row | inverted : row);
    }
    if (view.active.empty()) rows.push_back(text("  No active requests. Waiting for patient windows or the generator.") | dim);
    return vbox(std::move(rows));
}

Element hospitalTable(const AdminView& view) {
    Elements rows;
    rows.push_back(hbox({ui::cell("Hospital", 35), ui::cell("ALS free", 9), ui::cell("BLS free", 9), text("Total")}) | dim);
    for (const AdminView::HospitalRow& h : view.hospitals) {
        int free = h.freeAls + h.freeBls;
        rows.push_back(hbox({
            ui::cell(h.id + " " + h.name, 35),
            ui::cell(std::to_string(h.freeAls), 9),
            ui::cell(std::to_string(h.freeBls), 9),
            text(std::to_string(h.total)),
        }) | color(free == 0 ? Color::Red : Color(Color::Default)));
    }
    return vbox(std::move(rows));
}

Element ambulanceTable(const AdminView& view) {
    Elements rows;
    for (const AdminView::AmbulanceRow& a : view.ambulances) {
        Color c = a.status == "AVAILABLE" ? Color::Green : a.status == "AT_HOSPITAL" ? Color::Yellow : Color::Cyan;
        rows.push_back(hbox({
            ui::cell(a.id, 5),
            ui::cell(a.hospital, 4),
            ui::cell(a.type, 5),
            ui::cell(a.status, 13) | color(c),
            ui::cell(a.request.empty() ? "" : protocol::shortId(a.request), 9) | dim,
        }));
    }
    return vbox(std::move(rows));
}

Element checksPanel(const AdminView& view, int selected) {
    if (selected < 0 || selected >= static_cast<int>(view.active.size())) {
        return text("Choose a request with Up/Down to see how its hospital was chosen.") | dim;
    }
    const AdminView::RequestRow& r = view.active[selected];
    Elements rows;
    rows.push_back(text(r.injury + " at " + r.landmark + " - " + r.severity + ", priority " +
                        std::to_string(r.priorityScore)) |
                   bold);
    if (r.checks.empty()) rows.push_back(text("Not checked yet (waiting for the batch window).") | dim);
    for (const std::string& line : r.checks) {
        Color c = line.find("SELECTED") != std::string::npos ? Color::Green
                  : line.find("nearer hospital") != std::string::npos ? Color(Color::Default)
                                                                      : Color::Yellow;
        rows.push_back(paragraph(line) | color(c));
    }
    return vbox(std::move(rows));
}

}  // namespace

int main(int argc, char** argv) {
    ui::Options options(argc, argv);
    if (options.has("--help") || options.has("-h") || !options.problems().empty()) {
        for (const std::string& p : options.problems()) std::cout << p << "\n";
        printUsage();
        return options.problems().empty() ? 0 : 1;
    }

    bool ok = true;
    EngineSettings settings;
    double speed = options.getNumber("--speed", 60, &ok);
    settings.batchMinutes = options.getNumber("--batch", settings.batchMinutes, &ok);
    settings.returnTimeoutMinutes = options.getNumber("--timeout", settings.returnTimeoutMinutes, &ok);
    int startMinute = currentSystemMinuteOfDay();
    if (options.has("--time") && !parseClock(options.get("--time", ""), &startMinute)) ok = false;
    if (!ok || speed <= 0 || settings.batchMinutes < 0 || settings.returnTimeoutMinutes <= 0) {
        std::cout << "A number or time is wrong.\n";
        printUsage();
        return 1;
    }

    std::string dataDir = options.get("--data", "data");
    std::string sharedDir = options.get("--shared", "shared");
    World world;
    LoadReport report;
    if (!loadWorld(dataDir, world, report)) {
        std::cout << "Could not load the data in '" << dataDir << "':\n";
        for (const std::string& e : report.errors) std::cout << "  " << e << "\n";
        return 1;
    }

    AdminServer server(world, sharedDir, settings);
    std::string error;
    if (!server.start(&error)) {
        std::cout << "Could not start: " << error << "\n";
        return 1;
    }

    SimClock clock(startMinute, speed);
    std::mutex lock;
    AdminView view = server.view(clock.now());
    int selected = 0;

    ScreenInteractive screen = ScreenInteractive::Fullscreen();
    ui::Poller poller(screen, 500, [&] {
        double now = clock.now();
        server.step(now, speed);
        std::lock_guard<std::mutex> guard(lock);
        view = server.view(now);
        return true;  // the clock moves, so always redraw
    });

    Component root = Renderer([&] {
        std::lock_guard<std::mutex> guard(lock);
        if (selected >= static_cast<int>(view.active.size())) selected = static_cast<int>(view.active.size()) - 1;
        if (selected < 0) selected = 0;

        std::string speedText = std::to_string(static_cast<int>(speed));
        Element header = hbox({
            text(" RAHAT ") | bold | color(Color::Red),
            text("Admin / Server window") | bold,
            filler(),
            text("time " + view.clock + " (x" + speedText + ")   active " + std::to_string(view.active.size()) +
                 "   completed " + std::to_string(view.completed) + " "),
        });
        Element batch = view.batch.empty() ? text("") : text(" " + view.batch) | color(Color::Yellow);

        size_t shown = 10;
        std::vector<std::string> recent(view.log.size() > shown ? view.log.end() - shown : view.log.begin(),
                                        view.log.end());
        return vbox({
            header,
            batch,
            hbox({ui::titled("Queue - most urgent first", requestTable(view, selected)) | flex,
                  ui::titled("Hospitals", hospitalTable(view))}),
            hbox({ui::titled("Hospital selection for the chosen request", checksPanel(view, selected)) | flex,
                  ui::titled("Ambulances", ambulanceTable(view))}),
            ui::titled("Log", ui::lines(recent, dim)) | flex,
            text(" Up/Down: choose a request    q: quit   (start hospital and patient windows in other terminals)") |
                dim,
        });
    });

    root = CatchEvent(root, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        std::lock_guard<std::mutex> guard(lock);
        if (event == Event::ArrowDown) {
            if (selected + 1 < static_cast<int>(view.active.size())) ++selected;
            return true;
        }
        if (event == Event::ArrowUp) {
            if (selected > 0) --selected;
            return true;
        }
        return event == Event::Custom;
    });

    screen.Loop(root);
    return 0;
}
