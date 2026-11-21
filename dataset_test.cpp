// Tests for the DATASET part. Owner: Abhinav.
// Run from the phase2 folder: make test   (or: ./build/dataset_test)
#include <cstdlib>
#include <fstream>

#include "../common/check.h"
#include "csv.h"
#include "dataset.h"

using namespace phase2;

// Writes a small file used by the "bad data" tests.
static void writeFile(const std::string& path, const std::string& text) {
    std::ofstream file(path);
    file << text;
}

int main() {
    std::string error;

    std::cout << "== CSV reader ==\n";
    std::system("mkdir -p build/testdata");
    writeFile("build/testdata/sample.csv",
              "\xEF\xBB\xBF# comment line\r\n"
              "id,text,list\r\n"
              "\r\n"
              "1,\"Slow, small cut\",ICU|CT_SCAN\r\n");
    CsvTable table;
    CHECK(readCsv("build/testdata/sample.csv", &table, &error), "reads a Windows file with a BOM and comments");
    CHECK(table.header.size() == 3 && table.rows.size() == 1, "1 header row and 1 data row (blank line skipped)");
    CHECK(table.get(0, table.column("text")) == "Slow, small cut", "keeps the comma inside quotes");
    CHECK(splitList(table.get(0, 2)) == std::vector<std::string>({"ICU", "CT_SCAN"}), "splits ICU|CT_SCAN");
    CHECK(!readCsv("build/testdata/missing.csv", &table, &error), "a missing file is reported");

    std::cout << "\n== Real data files ==\n";
    Dataset data;
    bool loaded = loadDataset("../phase2_data", &data, &error);
    CHECK(loaded, "loads all 6 files from ../data " + error);
    if (loaded) {
        CHECK(data.landmarks.size() == 16, "16 landmarks");
        CHECK(data.roads.size() == 22, "22 roads");
        CHECK(data.hospitals.size() == 5, "5 hospitals");
        CHECK(data.injuries.size() == 5, "5 injuries");
        CHECK(data.landmarkName(1) == "Clock Tower", "landmark 1 is Clock Tower");
        CHECK(data.roads[0].id == "R1" && data.roads[0].km == 0.8, "road R1 is 0.8 km");
        const Injury& bleeding = data.injuries[0];
        CHECK(bleeding.code == "BLEEDING" && bleeding.questions.size() == 3, "bleeding has 3 questions");
        CHECK(bleeding.questions[0].options[0].text == "Slow, small cut", "answer text with a comma is read whole");
        CHECK(bleeding.rules.front().minPoints == 0 && bleeding.rules.back().severity == Severity::Critical,
              "bleeding rules go from 0 points up to CRITICAL");
    }

    std::cout << "\n== Bad data is caught ==\n";
    std::system("mkdir -p build/baddata");
    writeFile("build/baddata/landmarks.csv", "id,name\n1,A\n2,B\n");
    writeFile("build/baddata/roads.csv", "id,from,to,distance_km\nR1,1,9,2.0\n");  // 9 does not exist
    CHECK(!loadDataset("build/baddata", &data, &error) && error.find("roads.csv row 1") != std::string::npos,
          "road to a missing landmark is reported with file and row");
    writeFile("build/baddata/roads.csv", "id,from,to,distance_km\nR1,1,2,two\n");
    CHECK(!loadDataset("build/baddata", &data, &error), "distance that is not a number is reported");
    writeFile("build/baddata/landmarks.csv", "id,name\n1,A\n1,B\n");
    CHECK(!loadDataset("build/baddata", &data, &error) && error.find("twice") != std::string::npos,
          "duplicate landmark id is reported");

    return test::finish("dataset");
}
