#include "storage_sqlite.h"
#include <stdexcept>
#include <fstream>
#include <unordered_set>

static void exec(sqlite3* db, const char* sql) {
  char* err = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "";
    sqlite3_free(err);
    throw std::runtime_error(msg);
  }
}

SQLiteStore::SQLiteStore(const std::string& dbPath) {
  if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("Failed to open sqlite db");
  }
  exec(db_, "PRAGMA journal_mode=WAL;");
  exec(db_, "PRAGMA synchronous=NORMAL;");
}

SQLiteStore::~SQLiteStore() {
  if (db_) sqlite3_close(db_);
}

void SQLiteStore::initSchema() {
  exec(db_, R"SQL(
CREATE TABLE IF NOT EXISTS file (
  id INTEGER PRIMARY KEY,
  path TEXT UNIQUE,
  hash TEXT
);
CREATE TABLE IF NOT EXISTS function (
  id INTEGER PRIMARY KEY,
  usr TEXT UNIQUE,
  name TEXT,
  qualname TEXT,
  file_id INTEGER,
  start_line INTEGER,
  end_line INTEGER,
  is_static INTEGER,
  return_type TEXT,
  FOREIGN KEY(file_id) REFERENCES file(id)
);
CREATE TABLE IF NOT EXISTS function_call (
  id INTEGER PRIMARY KEY,
  caller_usr TEXT,
  callee_usr TEXT,
  caller_name TEXT,
  callee_name TEXT,
  file_id INTEGER,
  line INTEGER,
  call_type TEXT,
  FOREIGN KEY(file_id) REFERENCES file(id)
);
CREATE INDEX IF NOT EXISTS idx_fn_file ON function(file_id);
CREATE INDEX IF NOT EXISTS idx_call_caller ON function_call(caller_usr);
CREATE INDEX IF NOT EXISTS idx_call_callee ON function_call(callee_usr);
)SQL");
}

int SQLiteStore::ensureFileId(const std::string& path) const {
  sqlite3_stmt* stmt = nullptr;

  // insert ignore
  sqlite3_prepare_v2(db_,
    "INSERT OR IGNORE INTO file(path, hash) VALUES(?, '')", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // select id
  sqlite3_prepare_v2(db_, "SELECT id FROM file WHERE path = ?", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
  int id = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return id;
}

void SQLiteStore::upsertTU(const IRTranslationUnit& tu) {
  exec(db_, "BEGIN;");

  // prepared statements
  sqlite3_stmt* insFn = nullptr;
  sqlite3_prepare_v2(db_, R"SQL(
INSERT OR IGNORE INTO function
(usr, name, qualname, file_id, start_line, end_line, is_static, return_type)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
)SQL", -1, &insFn, nullptr);

  sqlite3_stmt* insCall = nullptr;
  sqlite3_prepare_v2(db_, R"SQL(
INSERT INTO function_call
(caller_usr, callee_usr, caller_name, callee_name, file_id, line, call_type)
VALUES (?, ?, ?, ?, ?, ?, ?)
)SQL", -1, &insCall, nullptr);

  for (const auto& fn : tu.functions) {
    int fileId = ensureFileId(fn.filePath);

    sqlite3_reset(insFn);
    sqlite3_bind_text(insFn, 1, fn.usr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insFn, 2, fn.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insFn, 3, fn.qualname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insFn, 4, fileId);
    sqlite3_bind_int(insFn, 5, fn.startLine);
    sqlite3_bind_int(insFn, 6, fn.endLine);
    sqlite3_bind_int(insFn, 7, fn.isStatic ? 1 : 0);
    sqlite3_bind_text(insFn, 8, fn.returnType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(insFn);
  }

  // Phase1: calls는 중복 제거를 간단히 위해 key set 사용
  std::unordered_set<std::string> seen;
  seen.reserve(tu.calls.size() * 2);

  for (const auto& c : tu.calls) {
    std::string key = c.callerUSR + "->" + c.calleeUSR + "@" + c.filePath + ":" + std::to_string(c.line);
    if (!seen.insert(key).second) continue;

    int fileId = ensureFileId(c.filePath);

    sqlite3_reset(insCall);
    sqlite3_bind_text(insCall, 1, c.callerUSR.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insCall, 2, c.calleeUSR.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insCall, 3, c.callerName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insCall, 4, c.calleeName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insCall, 5, fileId);
    sqlite3_bind_int(insCall, 6, c.line);
    sqlite3_bind_text(insCall, 7, c.callType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(insCall);
  }

  sqlite3_finalize(insFn);
  sqlite3_finalize(insCall);

  exec(db_, "COMMIT;");
}

void SQLiteStore::exportCallGraphPuml(const std::string& outPath, int maxEdges) const {
  std::ofstream out(outPath, std::ios::binary);
  out << "@startuml\n";
  out << "skinparam linetype ortho\n";
  out << "hide empty members\n\n";

  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, R"SQL(
SELECT caller_name, callee_name
FROM function_call
WHERE caller_name IS NOT NULL AND callee_name IS NOT NULL
LIMIT ?
)SQL", -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, maxEdges);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* caller = (const char*)sqlite3_column_text(stmt, 0);
    const char* callee = (const char*)sqlite3_column_text(stmt, 1);
    if (!caller || !callee) continue;
    out << "\"" << caller << "\" --> \"" << callee << "\" : calls\n";
  }
  sqlite3_finalize(stmt);

  out << "\n@enduml\n";
}
