// Console module: small helpers for the terminal windows - colours, clearing
// the screen, and reading a numbered menu choice safely.
//
// Colours use ANSI escape codes (Mac/Linux terminals, Windows Terminal, and
// Windows 10+ consoles after setup()). Set the NO_COLOR environment variable
// to turn them off.
#pragma once

#include <iostream>
#include <string>

#include "../models/models.h"

namespace rahat {
namespace console {

void setup();  // call once at program start
void setColors(bool enabled);
bool colorsEnabled();

void clearScreen();
void sleepMs(int milliseconds);

std::string bold(const std::string& text);
std::string dim(const std::string& text);
std::string red(const std::string& text);
std::string green(const std::string& text);
std::string yellow(const std::string& text);
std::string blue(const std::string& text);
std::string cyan(const std::string& text);
std::string magenta(const std::string& text);

// CRITICAL red, SERIOUS yellow, MODERATE cyan, MINOR green.
std::string bySeverity(Severity severity, const std::string& text);

// Pads with spaces (or cuts with "~") to exactly `width` characters. Colour the result, not the input.
std::string fit(const std::string& text, size_t width);

// Asks until the user types a whole number in [min, max]. Returns -1 if input ends.
int readChoice(std::istream& in, std::ostream& out, const std::string& prompt, int min, int max);
int readChoice(const std::string& prompt, int min, int max);

void waitForEnter(const std::string& prompt = "Press Enter to continue...");

}  // namespace console
}  // namespace rahat
