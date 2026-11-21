// RAHAT request generator - sends many requests at once, for demos and tests.
// It replaces typing them into patient windows; the admin window handles them
// exactly like real requests. Start the admin window first.
//
//   ./build/generator --scenario scenarios/priority_demo.csv
//   ./build/generator --random 5 [--seed 42] [--interval 0]
//   common options: [--data DIR] [--shared DIR]
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include "../modules/data_io/data_io.h"
#include "generator.h"
#include "patient_client.h"
#include "protocol.h"

using namespace rahat;

namespace {

void printUsage() {
    std::cout << "Usage:\n"
                 "  generator --scenario FILE            send the requests listed in a CSV file\n"
                 "  generator --random N [--seed S] [--interval SECONDS]\n"
                 "                                       send N random requests (default seed 42,\n"
                 "                                       interval 0 = all at once)\n"
                 "  options: --data DIR (default data)   --shared DIR (default shared)\n"
                 "Scenario file columns: send_at,landmark,injury,patients,answers\n"
                 "  answers: one per question, separated by |, as the option text or its number (1 = first).\n"
                 "  Put the answers in double quotes if any answer contains a comma.\n";
}

std::string value(int argc, char** argv, const std::string& key, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == key) return argv[i + 1];
    }
    return fallback;
}

bool has(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == key) return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    if (has(argc, argv, "--help") || (!has(argc, argv, "--scenario") && !has(argc, argv, "--random"))) {
        printUsage();
        return has(argc, argv, "--help") ? 0 : 1;
    }
    std::string dataDir = value(argc, argv, "--data", "data");
    std::string sharedDir = value(argc, argv, "--shared", "shared");

    World world;
    LoadReport report;
    if (!loadWorld(dataDir, world, report)) {
        std::cout << "Could not load the data in '" << dataDir << "':\n";
        for (const std::string& e : report.errors) std::cout << "  " << e << "\n";
        return 1;
    }

    std::vector<ScenarioItem> items;
    if (has(argc, argv, "--scenario")) {
        std::vector<std::string> problems;
        std::string file = value(argc, argv, "--scenario", "");
        if (!loadScenario(file, world, &items, &problems)) {
            std::cout << "Problems in " << file << ":\n";
            for (const std::string& p : problems) std::cout << "  " << p << "\n";
            return 1;
        }
    } else {
        int count = 0;
        double interval = 0;
        unsigned seed = 42;
        try {
            count = std::stoi(value(argc, argv, "--random", "0"));
            interval = std::stod(value(argc, argv, "--interval", "0"));
            seed = static_cast<unsigned>(std::stoul(value(argc, argv, "--seed", "42")));
        } catch (...) {
            count = -1;
        }
        if (count < 1 || interval < 0) {
            std::cout << "--random needs a number of requests (1 or more) and --interval 0 or more.\n";
            return 1;
        }
        std::mt19937 random(seed);
        for (int i = 0; i < count; ++i) items.push_back(randomItem(world, random, i * interval));
    }

    PatientClient sender(sharedDir);
    if (!sender.adminRunning()) {
        std::cout << "The admin window is not running. Start it first: ./build/admin\n";
        return 1;
    }

    std::cout << "Sending " << items.size() << (items.size() == 1 ? " request" : " requests") << "...\n";
    auto start = std::chrono::steady_clock::now();
    for (const ScenarioItem& item : items) {
        auto due = start + std::chrono::milliseconds(static_cast<long>(item.sendAt * 1000));
        std::this_thread::sleep_until(due);
        PatientClient one(sharedDir);
        std::string id, error;
        if (!one.send(item.request, &id, &error)) {
            std::cout << "  could not send " << item.label << ": " << error << "\n";
            return 1;
        }
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::printf("  +%5.1fs  %s  %s\n", elapsed, protocol::shortId(id).c_str(), item.label.c_str());
        std::fflush(stdout);
    }
    std::cout << "Done. Watch the admin window; each hospital window shows its own patients.\n";
    return 0;
}
