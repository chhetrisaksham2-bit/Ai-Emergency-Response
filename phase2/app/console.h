
// Owner: Utkarsh Kashyap (Organisation and interlinking)
#pragma once

#include <string>
#include <vector>

namespace phase2 {
namespace console {

std::string red(const std::string& text);
std::string green(const std::string& text);
std::string yellow(const std::string& text);
std::string cyan(const std::string& text);
std::string bold(const std::string& text);
std::string dim(const std::string& text);

void clearScreen();
void title(const std::string& text);
void step(int number, int total, const std::string& text);


int readChoice(const std::string& prompt, int low, int high);


void numberedList(const std::vector<std::string>& items);


std::string nice(const std::string& code);


std::string fit(const std::string& text, size_t width);

}  
}
