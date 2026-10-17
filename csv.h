// CSV reader - turns a .csv file into rows of text.
// Owner: Abhinav (Dataset)
//
// Rules for the files in data/:
//   - the first non-comment line is the header (column names)
//   - lines starting with # are comments, blank lines are skipped
//   - a value with a comma inside goes in double quotes: "Slow, small cut"
#pragma once

#include <string>
#include <vector>

namespace phase2 {

struct CsvTable {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;

    // Position of a column by its name, or -1 if the column is missing.
    int column(const std::string& name) const;

    // Value of one cell, or "" if the row is too short.
    std::string get(size_t row, int column) const;
};

// Reads the whole file. Returns false and fills `error` if it cannot be read.
bool readCsv(const std::string& path, CsvTable* table, std::string* error);

// Splits "ICU|CT_SCAN" into {"ICU", "CT_SCAN"}.
std::vector<std::string> splitList(const std::string& text, char separator = '|');

// Removes spaces, tabs and line endings from both ends.
std::string trim(const std::string& text);

}  // namespace phase2
