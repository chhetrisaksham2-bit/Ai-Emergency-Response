// Loads and checks all the data files Phase 2 needs.
// Owner: Abhinav (Dataset)
//
//   data/landmarks.csv          -> Dataset::landmarks
//   data/roads.csv              -> Dataset::roads
//   data/hospitals.csv          -> Dataset::hospitals
//   data/injuries.csv           -> Dataset::injuries
//   data/injury_questions.csv   -> Injury::questions
//   data/severity_rules.csv     -> Injury::rules
//
// Checks made while loading (the error names the file and row):
//   - required columns exist, numbers are numbers
//   - no duplicate landmark ids
//   - every road and hospital uses a landmark that exists
//   - every question has at least 2 answers
//   - every injury has questions and a severity rule with min_points 0
#pragma once

#include <string>

#include "records.h"

namespace phase2 {

bool loadDataset(const std::string& dataDir, Dataset* dataset, std::string* error);

}  // namespace phase2
