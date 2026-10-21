#include "console.h"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace phase2 {
namespace console {

namespace {

bool colours() {
    static bool on = std::getenv("NO_COLOR") == nullptr;
    return on;
}
std::string paint(const char* code, const std::string& text) {
    return colours() ? std::string("\033[") + code + "m" + text + "\033[0m" : text;
}
}

std::string red(const std::string& t) { return paint("31", t); }
std::string green(const std::string& t) { return paint("32", t); }
std::string yellow(const std::string& t) { return paint("33", t); }
std::string cyan(const std::string& t) { return paint("36", t); }
std::string bold(const std::string& t) { return paint("1", t); }
std::string dim(const std::string& t) { return paint("2", t); }

void clearScreen() {
    if (colours()) std::cout << "\033[H\033[2J\033[3J" << std::flush;
    else std::cout << "\n\n";
}

void title(const std::string& text) {
    std::cout << bold(text) << "\n" << std::string(text.size(), '-') << "\n";
}

void step(int number, int total, const std::string& text) {
    std::cout << "\n" << cyan("Step " + std::to_string(number) + " of " + std::to_string(total) + ": ") << bold(text)
              << "\n";
}

int readChoice(const std::string& prompt, int low, int high) {
    while (true) {
        std::cout << prompt << " [" << low << "-" << high << "]: " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return -1;  // input ended (Ctrl+D)
        std::stringstream in(line);
        int value;
        char extra;
        if (in >> value && !(in >> extra) && value >= low && value <= high) return value;
        std::cout << yellow("  Please type a number from " + std::to_string(low) + " to " + std::to_string(high) + ".")
                  << "\n";
    }
}

void numberedList(const std::vector<std::string>& items) {
    for (size_t i = 0; i < items.size(); ++i) std::cout << "  " << (i + 1) << ". " << items[i] << "\n";
}

std::string nice(const std::string& code) {
    if (code.empty()) return "-";
    std::string result;
    std::string word;
    std::stringstream words(code);
    while (std::getline(words, word, '_')) {
        std::string upper = word;
        for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        bool keepUpper = upper == "ICU" || upper == "CT" || upper == "ALS" || upper == "BLS";
        if (!keepUpper) {
            for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!word.empty()) word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
        }
        result += (result.empty() ? "" : " ") + (keepUpper ? upper : word);
    }
    return result;
}

std::string fit(const std::string& text, size_t width) {
    if (text.size() >= width) return text.substr(0, width);
    return text + std::string(width - text.size(), ' ');
}

}
}  
