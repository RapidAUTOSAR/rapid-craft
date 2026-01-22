#pragma once

#include "ir/sud/SudModel.h"
#include "ir.h"
#include <string>
#include <vector>

struct ClangTUInput {
  std::string sourcePath;
  std::vector<std::string> args;
};

class ClangExtractor {
public:
  IRTranslationUnit parse(const ClangTUInput& in);
};
