#pragma once
#include "ir.h"
#include <string>
#include <sqlite3.h>

class SQLiteStore {
public:
  explicit SQLiteStore(const std::string& dbPath);
  ~SQLiteStore();

  void initSchema();

  void upsertTU(const IRTranslationUnit& tu); // Phase1: 단순 insert (중복 방지 포함)
  void exportCallGraphPuml(const std::string& outPath, int maxEdges = 2000) const;

private:
  sqlite3* db_ = nullptr;

  int ensureFileId(const std::string& path) const;
};
