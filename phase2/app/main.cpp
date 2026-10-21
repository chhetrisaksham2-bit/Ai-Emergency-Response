// RAHAT Phase 2 - the first window: one emergency, from report to hospital.
// Owner: Utkarsh Kashyap (Organisation and interlinking)
//
// This file links the three other parts together:
//
//   dataset/          (Abhinav)        loadDataset()  -> records from the CSV files
//   datastructures/   (Utkarsh Tyagi)  Graph, MinHeap, dijkstra()   (used inside the system)
//   system/           (Saksham)        RahatSystem: assess() and rankHospitals()
//   app/              (Utkarsh Kashyap) this window: asks the questions, shows the answer
//
//   Step 1  What happened?          -> dataset: list of injuries
//   Step 2  Intensity questions     -> system: assess() -> severity
//   Step 3  Where is the patient?   -> dataset: list of landmarks
//   Step 4  Result                  -> system: rankHospitals() + best()
//
// Not in Phase 2: several requests at once, traffic, doctors, ambulance movement.
// Run from the phase2 folder:  ./build/rahat_phase2   (or: --data PATH)
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "../dataset/dataset.h"
#include "../system/rahat_system.h"
#include "console.h"

using namespace phase2;

namespace {

std::string kmText(double km) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f km", km);
    return buffer;
}

std::string severityText(Severity severity) {
    std::string text = toString(severity);
    switch (severity) {
        case Severity::Critical: return console::red(console::bold(text));
        case Severity::Serious: return console::red(text);
        case Severity::Moderate: return console::yellow(text);
        case Severity::Minor: return console::green(text);
    }
    return text;
}

std::string facilityList(const std::vector<std::string>& facilities) {
    if (facilities.empty()) return "nothing special";
    std::string text;
    for (const std::string& f : facilities) text += (text.empty() ? "" : ", ") + console::nice(f);
    return text;
}

std::string routeText(const Dataset& data, const std::vector<int>& route) {
    if (route.size() == 1) return data.landmarkName(route[0]) + " (the hospital is right here)";
    std::string text;
    for (int id : route) text += (text.empty() ? "" : "  ->  ") + data.landmarkName(id);
    return text;
}

// Landmarks in two columns; returns the chosen landmark id, or -1 to cancel.
int chooseLandmark(const Dataset& data) {
    const std::vector<Landmark>& all = data.landmarks;
    size_t half = (all.size() + 1) / 2;
    for (size_t row = 0; row < half; ++row) {
        std::cout << "  " << console::fit(std::to_string(row + 1) + ". " + all[row].name, 32);
        if (row + half < all.size()) std::cout << (row + half + 1) << ". " << all[row + half].name;
        std::cout << "\n";
    }
    int choice = console::readChoice("Nearest landmark (0 = cancel)", 0, static_cast<int>(all.size()));
    return choice <= 0 ? -1 : all[choice - 1].id;
}

void showResult(const RahatSystem& rahat, const Injury& injury, int landmark, const Assessment& assessment) {
    const Dataset& data = rahat.dataset();
    std::vector<HospitalOption> ranked = rahat.rankHospitals(landmark, assessment.facilities);
    const HospitalOption* chosen = RahatSystem::best(ranked);

    console::clearScreen();
    console::title("RAHAT  -  Result");
    std::cout << "  Patient     : " << injury.name << " at " << data.landmarkName(landmark) << "\n"
              << "  Severity    : " << severityText(assessment.severity) << "  (" << assessment.points << " points)\n"
              << "  Needs       : " << facilityList(assessment.facilities) << "\n"
              << "  Ambulance   : " << assessment.ambulance
              << (assessment.ambulance == "ALS" ? "  (Advanced Life Support)" : "  (Basic Life Support)") << "\n\n";

    if (chosen == nullptr) {
        std::cout << console::red(console::bold("  No reachable hospital has everything this patient needs.")) << "\n";
    } else {
        std::cout << console::green(console::bold("  SEND THE PATIENT TO: " + chosen->hospital->name)) << "\n"
                  << "  Distance    : " << kmText(chosen->km) << "\n"
                  << "  Route       : " << routeText(data, chosen->route) << "\n";
    }

    std::cout << "\n" << console::bold("  All hospitals, nearest first") << "\n"
              << console::dim("  " + console::fit("Hospital", 34) + console::fit("Distance", 11) + "Can treat?") << "\n";
    for (const HospitalOption& option : ranked) {
        std::string verdict = option.canTreat ? console::green("yes")
                                              : console::yellow("no - missing " + facilityList(option.missing));
        std::string marker = (&option == chosen) ? console::green(" <- chosen") : "";
        std::cout << "  " << console::fit(option.hospital->name, 34) << console::fit(kmText(option.km), 11) << verdict
                  << marker << "\n";
    }
    std::cout << "\n" << console::dim("  Distance = shortest road distance, found with Dijkstra's algorithm.") << "\n";
}

// One emergency from start to finish. Returns false if cancelled.
bool handleEmergency(const RahatSystem& rahat) {
    const Dataset& data = rahat.dataset();
    console::clearScreen();
    std::cout << console::red(console::bold("RAHAT")) << console::bold("  -  Report an emergency") << "\n"
              << std::string(60, '=') << "\n";

    console::step(1, 4, "What happened?");
    std::vector<std::string> names;
    for (const Injury& injury : data.injuries) names.push_back(injury.name);
    console::numberedList(names);
    int pick = console::readChoice("Injury (0 = cancel)", 0, static_cast<int>(names.size()));
    if (pick <= 0) return false;
    const Injury& injury = data.injuries[pick - 1];

    console::step(2, 4, "A few questions about the patient");
    std::vector<int> answers;
    for (const Question& question : injury.questions) {
        std::cout << "\n  " << console::bold(question.text) << "\n";
        std::vector<std::string> options;
        for (const AnswerOption& option : question.options) options.push_back(option.text);
        console::numberedList(options);
        int answer = console::readChoice("Answer", 1, static_cast<int>(options.size()));
        if (answer < 0) return false;
        answers.push_back(answer - 1);
    }
    Assessment assessment;
    std::string error;
    if (!rahat.assess(injury, answers, &assessment, &error)) {
        std::cout << console::red(error) << "\n";
        return false;
    }
    std::cout << "\n  Severity: " << severityText(assessment.severity) << "  (" << assessment.points << " points)\n";

    console::step(3, 4, "Where is the patient? Choose the nearest landmark");
    int landmark = chooseLandmark(data);
    if (landmark < 0) return false;

    showResult(rahat, injury, landmark, assessment);  // step 4
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string dataDir = "../phase2_data";
    if (argc == 3 && std::string(argv[1]) == "--data") dataDir = argv[2];

    Dataset data;
    RahatSystem rahat;
    std::string error;
    if (!loadDataset(dataDir, &data, &error) || !rahat.build(data, &error)) {
        std::cout << console::red("Could not start: " + error) << "\n"
                  << "Run this from the phase2 folder, or give the data folder: --data PATH\n";
        return 1;
    }

    while (true) {
        handleEmergency(rahat);
        std::cout << "\n  1. Report another emergency\n  0. Exit\n";
        if (console::readChoice("Choose", 0, 1) != 1) break;
    }
    std::cout << "Goodbye.\n";
    return 0;
}
