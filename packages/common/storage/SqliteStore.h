#pragma once

#include <string>
#include "ir/sud/SudModel.h"

class SqliteStore {
public:
  explicit SqliteStore(const std::string& dbPath);
  ~SqliteStore();

  /* schema */
  void initSchema();

  /* write (SudModel 단위) */
  void insertFunctions(const std::vector<SudFunction>& funcs);
  void insertCalls(const std::vector<SudCall>& calls);

  /* read */
  SudModel loadSudModel() const;

private:
  void* db_;
};
