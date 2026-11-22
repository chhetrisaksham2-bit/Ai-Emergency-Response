#include "console.h"

#include <chrono>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace rahat {
namespace console {

namespace {

bool& colorFlag() {
    static bool enabled = true;
    return enabled;
}

std::string paint(const char* code, const std::string& text) {
    if (!colorFlag()) return text;
    return std::string("\033[") + code + "m" + text + "\033[0m";
}

}  // namespace

void setup() {
    if (std::getenv("NO_COLOR") != nullptr) setColors(false);
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (output == INVALID_HANDLE_VALUE || !GetConsoleMode(output, &mode) ||
        !SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        setColors(false);  // very old console: plain text
    }
    SetConsoleOutputCP(65001);  // UTF-8
#endif
}

void setColors(bool enabled) { colorFlag() = enabled; }
bool colorsEnabled() { return colorFlag(); }

void clearScreen() {
    if (colorFlag()) {
        std::cout << "\033[2J\033[H" << std::flush;
    } else {
        std::cout << "\n\n----------------------------------------------------------------\n";
    }
}

void sleepMs(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

std::string bold(const std::string& text) { return paint("1", text); }
std::string dim(const std::string& text) { return paint("2", text); }
std::string red(const std::string& text) { return paint("1;31", text); }
std::string green(const std::string& text) { return paint("32", text); }
std::string yellow(const std::string& text) { return paint("33", text); }
std::string blue(const std::string& text) { return paint("34", text); }
std::string cyan(const std::string& text) { return paint("36", text); }
std::string magenta(const std::string& text) { return paint("35", text); }

std::string bySeverity(Severity severity, const std::string& text) {
    switch (severity) {
        case Severity::Critical: return red(text);
        case Severity::Serious: return yellow(text);
        case Severity::Moderate: return cyan(text);
        case Severity::Minor: return green(text);
    }
    return text;
}

std::string fit(const std::string& text, size_t width) {
    if (width == 0) return "";
    if (text.size() > width) return text.substr(0, width - 1) + "~";
    return text + std::string(width - text.size(), ' ');
}

int readChoice(std::istream& in, std::ostream& out, const std::string& prompt, int min, int max) {
    std::string line;
    while (true) {
        out << prompt << " [" << min << "-" << max << "]: " << std::flush;
        if (!std::getline(in, line)) return -1;
        try {
            size_t used = 0;
            int value = std::stoi(line, &used);
            while (used < line.size() && (line[used] == ' ' || line[used] == '\r')) ++used;
            if (used == line.size() && value >= min && value <= max) return value;
        } catch (...) {
        }
        out << "  Please type a number from " << min << " to " << max << ".\n";
    }
}

int readChoice(const std::string& prompt, int min, int max) { return readChoice(std::cin, std::cout, prompt, min, max); }

void waitForEnter(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

}  // namespace console
}  // namespace rahat
