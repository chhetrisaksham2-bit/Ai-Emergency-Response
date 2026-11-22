#include "comms.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace rahat {

namespace {

std::string escapeValue(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c != '\r') out += c;
    }
    return out;
}

std::string unescapeValue(const std::string& value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            char next = value[++i];
            out += next == 'n' ? '\n' : next;
        } else {
            out += value[i];
        }
    }
    return out;
}

bool parseInt(const std::string& text, int* out) {
    try {
        size_t used = 0;
        int number = std::stoi(text, &used);
        if (used != text.size()) return false;
        *out = number;
        return true;
    } catch (...) {
        return false;
    }
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

// ------------------------------------------------------------ Message

void Message::set(const std::string& key, const std::string& value) {
    if (!values.contains(key)) keys.push_back(key);
    values.put(key, value);
}

std::string Message::get(const std::string& key, const std::string& fallback) const {
    const std::string* value = values.find(key);
    return value == nullptr ? fallback : *value;
}

int Message::getInt(const std::string& key, int fallback) const {
    const std::string* value = values.find(key);
    int number = 0;
    return value != nullptr && parseInt(*value, &number) ? number : fallback;
}

std::string Message::serialize() const {
    std::string text;
    for (const std::string& key : keys) {
        text += key + "=" + escapeValue(*values.find(key)) + "\n";
    }
    return text;
}

bool Message::parse(const std::string& text, Message* out, std::string* error) {
    std::istringstream lines(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(lines, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t equals = line.find('=');
        if (equals == std::string::npos || equals == 0) {
            *error = "Line " + std::to_string(lineNumber) + " is not key=value";
            return false;
        }
        out->set(line.substr(0, equals), unescapeValue(line.substr(equals + 1)));
    }
    return true;
}

// ------------------------------------------------------------ Request conversion

Message requestToMessage(const Request& request) {
    Message message;
    message.set("id", request.id);
    message.set("injury", request.injuryCode);
    std::string answers;
    for (size_t i = 0; i < request.answers.size(); ++i) {
        if (i > 0) answers += ",";
        answers += std::to_string(request.answers[i]);
    }
    message.set("answers", answers);
    message.set("landmark", request.landmarkId);
    message.set("patients", request.patientCount);
    return message;
}

bool messageToRequest(const Message& message, Request* out, std::string* error) {
    for (const char* key : {"id", "injury", "answers", "landmark", "patients"}) {
        if (!message.has(key)) {
            *error = std::string("Request is missing '") + key + "'";
            return false;
        }
    }
    out->id = message.get("id");
    out->injuryCode = message.get("injury");
    if (out->id.empty() || !parseInt(message.get("landmark"), &out->landmarkId) ||
        !parseInt(message.get("patients"), &out->patientCount)) {
        *error = "Request has an empty id or a bad number";
        return false;
    }

    out->answers.clear();
    std::istringstream parts(message.get("answers"));
    std::string part;
    while (std::getline(parts, part, ',')) {
        int value = 0;
        if (!parseInt(part, &value)) {
            *error = "Bad answer value '" + part + "'";
            return false;
        }
        out->answers.push_back(value);
    }
    return true;
}

namespace {

std::string timeOrderedId(char letter) {
    using namespace std::chrono;
    long long millis = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    static std::mt19937 random(std::random_device{}());
    int suffix = static_cast<int>(random() % 1000);
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%03d", suffix);
    return std::string(1, letter) + std::to_string(millis) + "-" + buffer;
}

}  // namespace

std::string makeRequestId() { return timeOrderedId('R'); }
// One writer per folder, so a counter keeps this window's messages in order
// even when several are written in the same millisecond.
std::string makeMessageId() {
    static long counter = 0;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%06ld", ++counter % 1000000);
    std::string id = timeOrderedId('M');
    size_t dash = id.find('-');
    return id.substr(0, dash) + "-" + buffer + id.substr(dash);
}

// ------------------------------------------------------------ Channel

Channel::Channel(const std::string& sharedDirectory)
    : root(sharedDirectory),
      requests(fs::path(sharedDirectory) / "requests"),
      updates(fs::path(sharedDirectory) / "updates"),
      hospitals(fs::path(sharedDirectory) / "hospitals") {}

fs::path Channel::fromAdmin(const std::string& hospitalId) const { return hospitals / hospitalId / "from_admin"; }
fs::path Channel::toAdmin(const std::string& hospitalId) const { return hospitals / hospitalId / "to_admin"; }

bool Channel::open(std::string* error) {
    std::error_code code;
    for (const fs::path& folder : {requests, updates, hospitals}) {
        fs::create_directories(folder, code);
        if (code) {
            *error = "Cannot create shared folder " + folder.string() + ": " + code.message();
            return false;
        }
    }
    return true;
}

bool Channel::startSession(const std::vector<std::string>& hospitalIds, std::string* sessionId,
                           std::string* error) {
    std::error_code code;
    for (const fs::path& folder : {requests, updates, hospitals}) fs::remove_all(folder, code);
    fs::remove(root / "clock.txt", code);
    if (!open(error)) return false;
    for (const std::string& id : hospitalIds) {
        fs::create_directories(fromAdmin(id), code);
        if (!code) fs::create_directories(toAdmin(id), code);
        if (code) {
            *error = "Cannot create folder for hospital " + id + ": " + code.message();
            return false;
        }
    }
    *sessionId = makeMessageId();
    return writeAtomically(root / "session.txt", *sessionId + "\n", error);
}

std::string Channel::currentSession() const {
    std::string content;
    if (!readWhole(root / "session.txt", &content)) return "";
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r')) content.pop_back();
    return content;
}

bool Channel::writeClock(double simulatedMinutes, double speed, std::string* error) {
    Message message;
    message.set("minutes", std::to_string(simulatedMinutes));
    message.set("speed", std::to_string(speed));
    return writeAtomically(root / "clock.txt", message.serialize(), error);
}

bool Channel::readClock(double* simulatedMinutes, double* speed) const {
    std::string content;
    std::string error;
    Message message;
    if (!readWhole(root / "clock.txt", &content) || !Message::parse(content, &message, &error)) return false;
    try {
        *simulatedMinutes = std::stod(message.get("minutes", "0"));
        *speed = std::stod(message.get("speed", "1"));
    } catch (...) {
        return false;
    }
    return true;
}

bool Channel::writeAtomically(const fs::path& finalPath, const std::string& content, std::string* error) {
    fs::path temporary = finalPath;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary);
        if (!file) {
            *error = "Cannot write " + temporary.string();
            return false;
        }
        file << content;
    }
    std::error_code code;
    fs::rename(temporary, finalPath, code);
    if (code) {
        *error = "Cannot rename " + temporary.string() + ": " + code.message();
        return false;
    }
    return true;
}

bool Channel::readWhole(const fs::path& path, std::string* content) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    *content = buffer.str();
    return true;
}

std::vector<Message> Channel::readNumbered(const fs::path& folder, const std::string& prefix, int* nextSeq) {
    std::vector<Message> messages;
    while (true) {
        fs::path path = folder / (prefix + std::to_string(*nextSeq) + ".txt");
        std::string content;
        if (!fs::exists(path) || !readWhole(path, &content)) break;
        Message message;
        std::string error;
        if (Message::parse(content, &message, &error)) messages.push_back(message);
        ++*nextSeq;
    }
    return messages;
}

std::vector<Message> Channel::collect(const fs::path& folder, const std::string& prefix,
                                      std::vector<std::string>* problems) {
    std::vector<fs::path> files;
    std::error_code code;
    for (fs::directory_iterator it(folder, code), end; !code && it != end; it.increment(code)) {
        std::string name = it->path().filename().string();
        if (it->is_regular_file() && startsWith(name, prefix) && endsWith(name, ".txt")) files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());  // ids start with the time, so this is arrival order

    std::vector<Message> messages;
    for (const fs::path& path : files) {
        std::string content;
        std::string error;
        Message message;
        if (!readWhole(path, &content)) {
            problems->push_back("Cannot read " + path.filename().string());
        } else if (!Message::parse(content, &message, &error)) {
            problems->push_back(path.filename().string() + ": " + error);
        } else {
            messages.push_back(message);
        }
        fs::remove(path, code);
    }
    return messages;
}

std::vector<Message> Channel::collectRequests(std::vector<std::string>* problems) {
    return collect(requests, "req_", problems);
}

bool Channel::sendUpdate(const std::string& requestId, int seq, const Message& message, std::string* error) {
    return writeAtomically(updates / (requestId + "_" + std::to_string(seq) + ".txt"), message.serialize(), error);
}

bool Channel::sendToHospital(const std::string& hospitalId, int seq, const Message& message, std::string* error) {
    std::error_code code;
    fs::create_directories(fromAdmin(hospitalId), code);
    return writeAtomically(fromAdmin(hospitalId) / (std::to_string(seq) + ".txt"), message.serialize(), error);
}

std::vector<Message> Channel::collectFromHospital(const std::string& hospitalId,
                                                  std::vector<std::string>* problems) {
    return collect(toAdmin(hospitalId), "msg_", problems);
}

bool Channel::sendRequest(const std::string& requestId, const Message& message, std::string* error) {
    return writeAtomically(requests / ("req_" + requestId + ".txt"), message.serialize(), error);
}

std::vector<Message> Channel::readUpdates(const std::string& requestId, int* nextSeq) const {
    return readNumbered(updates, requestId + "_", nextSeq);
}

std::vector<Message> Channel::readFromAdmin(const std::string& hospitalId, int* nextSeq) const {
    return readNumbered(fromAdmin(hospitalId), "", nextSeq);
}

bool Channel::sendToAdmin(const std::string& hospitalId, const Message& message, std::string* error) {
    std::error_code code;
    fs::create_directories(toAdmin(hospitalId), code);
    return writeAtomically(toAdmin(hospitalId) / ("msg_" + makeMessageId() + ".txt"), message.serialize(), error);
}

}  // namespace rahat
