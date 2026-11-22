#include "ui_common.h"

#include <algorithm>
#include <chrono>

#include <ftxui/component/event.hpp>
#include <ftxui/screen/terminal.hpp>

namespace rahat {
namespace ui {

using namespace ftxui;

Options::Options(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) != 0) {
            errors.push_back("Unexpected argument '" + key + "'");
            continue;
        }
        if (key == "--help" || key == "-h" || i + 1 >= argc || std::string(argv[i + 1]).rfind("--", 0) == 0) {
            values.push_back({key, ""});  // a flag without a value
            continue;
        }
        values.push_back({key, argv[++i]});
    }
}

bool Options::has(const std::string& key) const {
    for (const auto& kv : values) {
        if (kv.first == key) return true;
    }
    return false;
}

std::string Options::get(const std::string& key, const std::string& fallback) const {
    for (const auto& kv : values) {
        if (kv.first == key && !kv.second.empty()) return kv.second;
    }
    return fallback;
}

double Options::getNumber(const std::string& key, double fallback, bool* ok) const {
    std::string text = get(key, "");
    if (text.empty()) return fallback;
    try {
        size_t used = 0;
        double value = std::stod(text, &used);
        if (used == text.size()) return value;
    } catch (...) {
    }
    *ok = false;
    return fallback;
}

Poller::Poller(ScreenInteractive& screen, int periodMs, std::function<bool()> work)
    : thread([this, &screen, periodMs, work] {
          while (running) {
              if (work()) screen.PostEvent(Event::Custom);
              for (int waited = 0; running && waited < periodMs; waited += 50) {
                  std::this_thread::sleep_for(std::chrono::milliseconds(50));
              }
          }
      }) {}

Poller::~Poller() {
    running = false;
    if (thread.joinable()) thread.join();
}

Color severityColor(const std::string& severity) {
    if (severity == "CRITICAL") return Color::Red;
    if (severity == "SERIOUS") return Color::Yellow;
    if (severity == "MODERATE") return Color::Cyan;
    if (severity == "MINOR") return Color::Green;
    return Color::Default;
}

Element severityLabel(const std::string& severity) {
    Element label = text(severity) | color(severityColor(severity));
    return severity == "CRITICAL" ? label | bold : label;
}

Element lines(const std::vector<std::string>& textLines, Decorator style) {
    Elements rows;
    for (const std::string& line : textLines) rows.push_back(paragraph(line) | style);
    if (rows.empty()) rows.push_back(text("-") | dim);
    return vbox(std::move(rows));
}

Element titled(const std::string& title, Element body) { return window(text(" " + title + " ") | bold, body); }

std::string fit(const std::string& text, size_t width) {
    if (text.size() > width) return width == 0 ? "" : text.substr(0, width - 1) + "~";
    return text + std::string(width - text.size(), ' ');
}

Element cell(const std::string& value, int width) {
    return text(fit(value, static_cast<size_t>(width))) | size(WIDTH, EQUAL, width);
}

void mapSize(int roomColumns, int* columns, int* rows) {
    Dimensions screen = Terminal::Size();
    *columns = std::max(30, std::min(64, roomColumns));
    *rows = std::max(12, std::min(28, screen.dimy - 10));
}

}  // namespace ui
}  // namespace rahat
