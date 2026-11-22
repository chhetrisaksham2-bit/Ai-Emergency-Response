// RAHAT patient window.
//
// Asks what happened, the follow-up questions for that injury, where the
// patient is and how many patients there are, then sends the request to the
// admin window and shows the live updates. The final screen shows the
// destination hospital and every hospital that was checked, with the reason
// each one was or was not selected.
//
//   ./build/patient [--data DIR] [--shared DIR]
#include <iostream>
#include <mutex>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include "../modules/data_io/data_io.h"
#include "map_view.h"
#include "patient_client.h"
#include "protocol.h"
#include "ui_common.h"

using namespace rahat;
using namespace ftxui;

namespace {

enum Stage { PickInjury = 0, AnswerQuestions = 1, PickLandmark = 2, PickPatients = 3, Tracking = 4 };

Color updateColor(const std::string& type) {
    if (type == "COMPLETED" || type == "ARRIVED_AT_HOSPITAL") return Color::Green;
    if (type == "WAITING" || type == "INVALID") return Color::Red;
    if (type == "DISPATCHED" || type == "ARRIVED_AT_SCENE" || type == "LEFT_SCENE") return Color::Cyan;
    return Color(Color::Default);
}

}  // namespace

int main(int argc, char** argv) {
    ui::Options options(argc, argv);
    if (options.has("--help") || !options.problems().empty()) {
        for (const std::string& p : options.problems()) std::cout << p << "\n";
        std::cout << "Usage: patient [--data DIR] [--shared DIR]\n";
        return options.problems().empty() ? 0 : 1;
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
    const std::vector<Injury*>& injuries = world.injuries.all();
    CityPicture picture;
    std::string mapError;
    bool hasMap = loadCityPicture(dataDir, &picture, &mapError);  // optional
    const std::vector<int>& landmarkIds = world.map.landmarkIds();

    PatientClient client(sharedDir);
    std::mutex lock;

    int stage = PickInjury;
    int injurySelected = 0, answerSelected = 0, landmarkSelected = 0, patientsSelected = 0;
    size_t questionIndex = 0;
    std::vector<int> answers;
    std::vector<std::string> answerEntries;
    std::string message;
    std::string label;  // "Stroke at Clock Tower"

    std::vector<std::string> injuryEntries;
    for (const Injury* injury : injuries) injuryEntries.push_back(injury->name);
    std::vector<std::string> landmarkEntries;
    for (int id : landmarkIds) {
        landmarkEntries.push_back((id < 10 ? " " : "") + std::to_string(id) + ". " + world.map.landmarkName(id));
    }
    std::vector<std::string> patientEntries;
    for (int n = 1; n <= 10; ++n) patientEntries.push_back(n == 1 ? "1 patient" : std::to_string(n) + " patients");

    ScreenInteractive screen = ScreenInteractive::Fullscreen();

    auto loadQuestion = [&] {
        const Question& q = injuries[injurySelected]->questions[questionIndex];
        answerEntries.clear();
        for (const AnswerOption& option : q.options) answerEntries.push_back(option.text);
        answerSelected = 0;
    };
    auto restart = [&] {
        stage = PickInjury;
        questionIndex = 0;
        answers.clear();
        message.clear();
        std::lock_guard<std::mutex> guard(lock);
        client.forget();
    };
    auto chooseInjury = [&] {
        message.clear();
        questionIndex = 0;
        answers.clear();
        loadQuestion();
        stage = AnswerQuestions;
    };
    auto answerQuestion = [&] {
        answers.push_back(answerSelected);
        if (++questionIndex >= injuries[injurySelected]->questions.size()) {
            stage = PickLandmark;
        } else {
            loadQuestion();
        }
    };
    auto chooseLandmark = [&] { stage = PickPatients; };
    auto send = [&] {
        Request request;
        request.injuryCode = injuries[injurySelected]->code;
        request.answers = answers;
        request.landmarkId = landmarkIds[landmarkSelected];
        request.patientCount = patientsSelected + 1;
        std::string id, error;
        std::lock_guard<std::mutex> guard(lock);
        if (!client.send(request, &id, &error)) {
            message = error;
            return;
        }
        label = injuries[injurySelected]->name + " at " + world.map.landmarkName(request.landmarkId);
        message.clear();
        stage = Tracking;
    };
    auto goBack = [&] {
        message.clear();
        if (stage == AnswerQuestions && questionIndex > 0) {
            --questionIndex;
            answers.pop_back();
            loadQuestion();
        } else if (stage == AnswerQuestions) {
            stage = PickInjury;
        } else if (stage == PickLandmark) {
            questionIndex = injuries[injurySelected]->questions.size() - 1;
            answers.pop_back();
            loadQuestion();
            stage = AnswerQuestions;
        } else if (stage == PickPatients) {
            stage = PickLandmark;
        }
    };

    ui::Poller poller(screen, 500, [&] {
        std::lock_guard<std::mutex> guard(lock);
        return client.poll();
    });

    auto menuWith = [](std::vector<std::string>* entries, int* selected, std::function<void()> onEnter) {
        MenuOption option = MenuOption::Vertical();
        option.on_enter = onEnter;
        return Menu(entries, selected, option);
    };
    Component injuryMenu = menuWith(&injuryEntries, &injurySelected, chooseInjury);
    Component answerMenu = menuWith(&answerEntries, &answerSelected, answerQuestion);
    Component landmarkMenu = menuWith(&landmarkEntries, &landmarkSelected, chooseLandmark);
    Component patientsMenu = menuWith(&patientEntries, &patientsSelected, send);
    Component trackingPage = Renderer([] { return emptyElement(); });
    Component pages = Container::Tab({injuryMenu, answerMenu, landmarkMenu, patientsMenu, trackingPage}, &stage);

    const char* stepTitles[] = {"What happened?", "A few questions about the patient",
                                "Where is the patient? Choose the nearest landmark", "How many patients are there?",
                                "Your request"};

    Component root = Renderer(pages, [&]() -> Element {
        std::lock_guard<std::mutex> guard(lock);
        int mapColumns = 0, mapRows = 0;
        Element header = hbox({
            text(" RAHAT ") | bold | color(Color::Red),
            text("Patient window - report an emergency") | bold,
            filler(),
            text(client.adminRunning() ? "time " + client.clock() + " " : "admin window not running ") |
                color(client.adminRunning() ? Color(Color::Default) : Color::Yellow),
        });
        std::string stepText =
            stage == Tracking ? std::string(stepTitles[4])
                              : "Step " + std::to_string(stage + 1) + " of 4: " + stepTitles[stage];

        if (stage != Tracking) {
            Elements body;
            if (stage == AnswerQuestions) {
                const Injury* injury = injuries[injurySelected];
                body.push_back(text(injury->name + " - question " + std::to_string(questionIndex + 1) + " of " +
                                    std::to_string(injury->questions.size())) |
                               dim);
                body.push_back(text(injury->questions[questionIndex].text) | bold);
            }
            body.push_back(pages->Render() | vscroll_indicator | yframe | flex);
            std::string hint = stage == PickPatients ? "Up/Down + Enter: send the request   Esc: back   q: quit"
                                                     : "Up/Down + Enter: choose   Esc: back   q: quit";
            Element choose = window(text(" Choose "), vbox(std::move(body))) | flex;
            if (hasMap && stage == PickLandmark) {  // the ring follows the highlighted landmark
                MapMarks marks;
                marks.patientLandmark = landmarkIds[landmarkSelected];
                ui::mapSize(Terminal::Size().dimx - 34 - 4, &mapColumns, &mapRows);
                choose = hbox({choose, window(text(" Map "), drawMap(picture, marks, mapColumns, mapRows))});
            }
            return vbox({header, text(" " + stepText) | bold | color(Color::Cyan), choose | flex,
                         message.empty() ? text("") : text(" " + message) | color(Color::Red), text(" " + hint) | dim});
        }

        // ---- Tracking: destination, hospitals checked, live updates
        Elements top;
        top.push_back(paragraph("Request " + protocol::shortId(client.requestId()) + ": " + label));
        if (!client.severity().empty()) top.push_back(hbox({text("Severity: "), ui::severityLabel(client.severity())}));
        if (!client.destination().empty()) {
            top.push_back(hbox({text("DESTINATION: ") | bold, text(client.destination()) | bold | color(Color::Green)}));
        } else if (client.lost()) {
            top.push_back(text("The admin window was restarted, so this request is gone. Press Enter to report again.") |
                          color(Color::Red));
        } else {
            top.push_back(text("Waiting for the system to choose a hospital...") | color(Color::Yellow));
        }

        Elements checks;
        for (const std::string& line : client.hospitalsChecked()) {
            Color c = line.find("SELECTED") != std::string::npos ? Color::Green
                      : line.find("nearer hospital") != std::string::npos ? Color(Color::Default)
                                                                          : Color::Yellow;
            checks.push_back(paragraph(line) | color(c));
        }
        if (checks.empty()) checks.push_back(text("Not checked yet.") | dim);

        Elements updates;
        for (const PatientUpdate& u : client.updates()) {
            if (u.type == "HOSPITALS_CHECKED") continue;  // shown in its own panel
            updates.push_back(hbox({text("[" + u.time + "] ") | dim, paragraph(u.text) | color(updateColor(u.type))}));
            for (const std::string& line : u.details) updates.push_back(text("          " + line) | dim);
        }
        if (updates.empty()) updates.push_back(text("Sent. Waiting for the admin window...") | dim);

        std::string hint = client.finished() || client.lost() ? "Enter: report another emergency   q: quit"
                                                              : "Updates appear by themselves   q: quit";
        Element summary = vbox({window(text(" Status "), vbox(top)),
                                window(text(" Hospitals checked for you, nearest first "), vbox(std::move(checks)))});
        if (hasMap) {
            MapMarks marks;
            marks.patientLandmark = landmarkIds[landmarkSelected];
            marks.route = routeFromLines(picture, client.route());
            auto hospital = picture.hospitalLandmark.find(client.destination());
            if (hospital != picture.hospitalLandmark.end()) marks.chosenHospitalLandmark = hospital->second;
            ui::mapSize(Terminal::Size().dimx - 40 - 4, &mapColumns, &mapRows);
            summary = hbox({summary | flex, window(text(" Map "), drawMap(picture, marks, mapColumns, mapRows))});
        }
        return vbox({header, text(" " + stepText) | bold | color(Color::Cyan), summary,
                     window(text(" Live updates "), vbox(std::move(updates)) | focusPositionRelative(0, 1) | vscroll_indicator | yframe) | flex,
                     text(" " + hint) | dim});
    });

    root = CatchEvent(root, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        if (event == Event::Custom) return true;
        if (stage == Tracking) {
            bool done;
            {
                std::lock_guard<std::mutex> guard(lock);
                done = client.finished() || client.lost();
            }
            if (event == Event::Return && done) restart();
            return true;  // the menus are hidden while tracking
        }
        if (event == Event::Escape) {
            goBack();
            return true;
        }
        return false;
    });

    screen.Loop(root);
    return 0;
}
