// Small pieces shared by the full-screen windows (admin, hospital, patient).
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace rahat {
namespace ui {

// "--key value" command-line options.
class Options {
public:
    Options(int argc, char** argv);
    bool has(const std::string& key) const;
    std::string get(const std::string& key, const std::string& fallback) const;
    double getNumber(const std::string& key, double fallback, bool* ok) const;
    const std::vector<std::string>& problems() const { return errors; }

private:
    std::vector<std::pair<std::string, std::string>> values;
    std::vector<std::string> errors;
};

// Runs `work` every `periodMs` milliseconds on a background thread. When it
// returns true, the screen is asked to redraw (Event::Custom). This is how the
// windows show new files without a refresh key.
class Poller {
public:
    Poller(ftxui::ScreenInteractive& screen, int periodMs, std::function<bool()> work);
    ~Poller();
    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;

private:
    std::atomic<bool> running{true};
    std::thread thread;
};

ftxui::Color severityColor(const std::string& severity);
ftxui::Element severityLabel(const std::string& severity);
ftxui::Element lines(const std::vector<std::string>& text, ftxui::Decorator style = ftxui::nothing);
ftxui::Element titled(const std::string& title, ftxui::Element body);
std::string fit(const std::string& text, size_t width);  // pad or cut to an exact width
// One table cell of an exact width, so columns line up and are never squeezed.
ftxui::Element cell(const std::string& text, int width);

// Map size, as in Phase 2: rows follow the terminal height (12 to 28), columns follow the
// room the window has for the map (30 to 64). roomColumns is the width free for the map.
void mapSize(int roomColumns, int* columns, int* rows);

}  // namespace ui
}  // namespace rahat
