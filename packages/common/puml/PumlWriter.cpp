#include "puml/PumlWriter.h"
#include <fstream>

void PumlWriter::begin() {
  lines_.push_back("@startuml");
}

void PumlWriter::arrow(const std::string& a, const std::string& b) {
  lines_.push_back("\"" + a + "\" -> \"" + b + "\"");
}

void PumlWriter::end() {
  lines_.push_back("@enduml");
}

void PumlWriter::save(const std::string& path) const {
  std::ofstream f(path);
  for (auto& l : lines_) f << l << "\n";
}
