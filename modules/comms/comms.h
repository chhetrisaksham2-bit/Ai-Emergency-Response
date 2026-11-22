// Comms module: how the RAHAT windows talk to each other.
//
// Each window is a separate program, so they exchange small text files in a
// shared folder. Every folder has exactly ONE writer:
//
//   session.txt                            admin -> everyone   id of the running admin session
//   clock.txt                              admin -> everyone   current simulated time
//   requests/req_<id>.txt                  patient / generator -> admin   (read and deleted)
//   updates/<id>_<seq>.txt                 admin -> patient    status update #seq for one request
//   hospitals/<H>/from_admin/<seq>.txt     admin -> hospital H message #seq (assignments, status)
//   hospitals/<H>/to_admin/msg_<id>.txt    hospital H -> admin (button presses; read and deleted)
//
// Every file is written as "<name>.tmp" first and then renamed, so the other
// program never reads a half-written file.
//
// The admin window starts a new session, which clears the shared folder. The
// other windows watch session.txt: when it changes they start again from #0.
//
// File format: one "key=value" per line. Newlines inside values are escaped.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../ds/hash_map.h"
#include "../models/models.h"

namespace rahat {

class Message {
public:
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, int value) { set(key, std::to_string(value)); }

    bool has(const std::string& key) const { return values.contains(key); }
    std::string get(const std::string& key, const std::string& fallback = "") const;
    int getInt(const std::string& key, int fallback) const;

    std::string serialize() const;  // keys in the order they were first set
    static bool parse(const std::string& text, Message* out, std::string* error);

private:
    HashMap<std::string, std::string> values;
    std::vector<std::string> keys;
};

// Request <-> Message, used by both windows.
Message requestToMessage(const Request& request);
bool messageToRequest(const Message& message, Request* out, std::string* error);

// Unique, time-ordered id such as R1757925123456-042.
std::string makeRequestId();

// Unique, time-ordered id for any file name, same shape as makeRequestId().
std::string makeMessageId();

class Channel {
public:
    explicit Channel(const std::string& sharedDirectory);

    // Creates the folders if needed (does not clear anything).
    bool open(std::string* error);

    // ---- Admin side
    // Clears the shared folder, creates a folder for every hospital, and writes
    // a new session id. Call once when the admin window starts.
    bool startSession(const std::vector<std::string>& hospitalIds, std::string* sessionId, std::string* error);
    // Reads and deletes every waiting request file, oldest id first.
    std::vector<Message> collectRequests(std::vector<std::string>* problems);
    bool sendUpdate(const std::string& requestId, int seq, const Message& message, std::string* error);
    bool sendToHospital(const std::string& hospitalId, int seq, const Message& message, std::string* error);
    // Reads and deletes every message the hospital sent, oldest first.
    std::vector<Message> collectFromHospital(const std::string& hospitalId, std::vector<std::string>* problems);

    // ---- Patient window and generator
    bool sendRequest(const std::string& requestId, const Message& message, std::string* error);
    // Returns updates #nextSeq, #nextSeq+1, ... that exist, and advances nextSeq.
    std::vector<Message> readUpdates(const std::string& requestId, int* nextSeq) const;

    // ---- Hospital window
    std::vector<Message> readFromAdmin(const std::string& hospitalId, int* nextSeq) const;
    bool sendToAdmin(const std::string& hospitalId, const Message& message, std::string* error);

    // Written by the admin every step so the other windows can show the same time.
    bool writeClock(double simulatedMinutes, double speed, std::string* error);

    // ---- Everyone
    std::string currentSession() const;  // empty if no admin session has started
    bool readClock(double* simulatedMinutes, double* speed) const;  // false if not written yet
    const std::filesystem::path& folder() const { return root; }

private:
    std::filesystem::path root;
    std::filesystem::path requests;
    std::filesystem::path updates;
    std::filesystem::path hospitals;

    std::filesystem::path fromAdmin(const std::string& hospitalId) const;
    std::filesystem::path toAdmin(const std::string& hospitalId) const;

    static bool writeAtomically(const std::filesystem::path& finalPath, const std::string& content,
                                std::string* error);
    static bool readWhole(const std::filesystem::path& path, std::string* content);
    // Numbered files <prefix><seq>.txt, read in order without deleting.
    static std::vector<Message> readNumbered(const std::filesystem::path& folder, const std::string& prefix,
                                             int* nextSeq);
    // Every <prefix>*.txt file, sorted by name, read and deleted.
    static std::vector<Message> collect(const std::filesystem::path& folder, const std::string& prefix,
                                        std::vector<std::string>* problems);
};

}  // namespace rahat
