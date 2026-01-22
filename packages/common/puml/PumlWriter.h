#pragma once
#include <string>
#include <vector>

class PumlWriter {
public:
  void begin();
  void arrow(const std::string& a, const std::string& b);
  void end();
  void save(const std::string& path) const;

private:
  std::vector<std::string> lines_;
};
