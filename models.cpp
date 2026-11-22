#include "models.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace rahat {

std::string toString(Severity severity) {
    switch (severity) {
        case Severity::Minor: return "MINOR";
        case Severity::Moderate: return "MODERATE";
        case Severity::Serious: return "SERIOUS";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

std::string toString(AmbulanceType type) {
    return type == AmbulanceType::ALS ? "ALS" : "BLS";
}

std::string toString(AmbulanceStatus status) {
    switch (status) {
        case AmbulanceStatus::Available: return "AVAILABLE";
        case AmbulanceStatus::ToScene: return "TO_SCENE";
        case AmbulanceStatus::AtScene: return "AT_SCENE";
        case AmbulanceStatus::ToHospital: return "TO_HOSPITAL";
        case AmbulanceStatus::AtHospital: return "AT_HOSPITAL";
    }
    return "UNKNOWN";
}

std::string toString(RequestStatus status) {
    switch (status) {
        case RequestStatus::Queued: return "QUEUED";
        case RequestStatus::Waiting: return "WAITING";
        case RequestStatus::InProgress: return "IN_PROGRESS";
        case RequestStatus::Completed: return "COMPLETED";
    }
    return "UNKNOWN";
}

bool parseSeverity(const std::string& text, Severity* out) {
    if (text == "MINOR") *out = Severity::Minor;
    else if (text == "MODERATE") *out = Severity::Moderate;
    else if (text == "SERIOUS") *out = Severity::Serious;
    else if (text == "CRITICAL") *out = Severity::Critical;
    else return false;
    return true;
}

bool parseAmbulanceType(const std::string& text, AmbulanceType* out) {
    if (text == "BLS") *out = AmbulanceType::BLS;
    else if (text == "ALS") *out = AmbulanceType::ALS;
    else return false;
    return true;
}

std::string humanize(const std::string& code) {
    if (code == "X_RAY") return "X-ray";
    // Short medical abbreviations stay in capitals: ICU, CT, ECG, ALS, BLS.
    std::string text;
    std::string word;
    for (size_t i = 0; i <= code.size(); ++i) {
        if (i < code.size() && code[i] != '_') {
            word += code[i];
            continue;
        }
        bool keep = word == "ICU" || word == "CT" || word == "ECG" || word == "ALS" || word == "BLS";
        if (!keep) {
            for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (!word.empty()) text += (text.empty() ? "" : " ") + word;
        word.clear();
    }
    return text;
}

bool parseClock(const std::string& text, int* minuteOfDay) {
    // Accepts H:MM or HH:MM, and 24:00 as end of day.
    size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon > 2 || text.size() != colon + 3) return false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (i != colon && !std::isdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    int hours = std::stoi(text.substr(0, colon));
    int minutes = std::stoi(text.substr(colon + 1));
    if (minutes > 59 || hours > 24 || (hours == 24 && minutes != 0)) return false;
    *minuteOfDay = hours * 60 + minutes;
    return true;
}

int minuteOfDay(double minutes) {
    long whole = static_cast<long>(std::floor(minutes));
    long wrapped = whole % 1440;
    if (wrapped < 0) wrapped += 1440;
    return static_cast<int>(wrapped);
}

std::string formatClock(double minutes) {
    int m = minuteOfDay(minutes);
    char buffer[16];  // "HH:MM" needs 6; extra room keeps the compiler's size check quiet
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", m / 60, m % 60);
    return buffer;
}

bool inTimeWindow(int minute, int start, int end) {
    if (start == end) return true;  // e.g. 00:00-24:00 stored as 0-0 means all day
    if (start < end) return minute >= start && minute < end;
    return minute >= start || minute < end;  // window crosses midnight
}

SimClock::SimClock(int startMinuteOfDay, double speed)
    : realStart(std::chrono::steady_clock::now()),
      startMinutes(startMinuteOfDay),
      speedFactor(speed) {}

double SimClock::now() const {
    double realSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - realStart).count();
    return startMinutes + realSeconds * speedFactor / 60.0;
}

int currentSystemMinuteOfDay() {
    std::time_t t = std::time(nullptr);
    std::tm* local = std::localtime(&t);
    return local->tm_hour * 60 + local->tm_min;
}

bool Hospital::hasFacility(const std::string& facility) const {
    for (const std::string& f : facilities) {
        if (f == facility) return true;
    }
    return false;
}

bool Hospital::hasAllFacilities(const std::vector<std::string>& required) const {
    for (const std::string& f : required) {
        if (!hasFacility(f)) return false;
    }
    return true;
}

}  // namespace rahat
