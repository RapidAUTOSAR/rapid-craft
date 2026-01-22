#include "storage/SqliteStore.h"

#include <sqlite3.h>
#include <stdexcept>
#include <iostream>

/* ============================================================
 * Constructor / Destructor
 * ============================================================ */

SqliteStore::SqliteStore(const std::string& dbPath)
  : db_(nullptr)
{
  if (sqlite3_open(dbPath.c_str(), reinterpret_cast<sqlite3**>(&db_)) != SQLITE_OK) {
    throw std::runtime_error("Failed to open SQLite DB: " + dbPath);
  }
}

SqliteStore::~SqliteStore()
{
  if (db_) {
    sqlite3_close(reinterpret_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

/* ============================================================
 * Schema (Indexer)
 * ============================================================ */

void SqliteStore::initSchema()
{
  const char* sql = R"(
    CREATE TABLE IF NOT EXISTS sud_function (
      usr        TEXT PRIMARY KEY,
      name       TEXT NOT NULL,
      file       TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS sud_call (
      caller_usr TEXT NOT NULL,
      callee_usr TEXT NOT NULL,
      FOREIGN KEY(caller_usr) REFERENCES sud_function(usr),
      FOREIGN KEY(callee_usr) REFERENCES sud_function(usr)
    );

    CREATE INDEX IF NOT EXISTS idx_sud_call_caller
      ON sud_call(caller_usr);

    CREATE INDEX IF NOT EXISTS idx_sud_call_callee
      ON sud_call(callee_usr);
  )";

  char* err = nullptr;
  if (sqlite3_exec(reinterpret_cast<sqlite3*>(db_), sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "Unknown error";
    sqlite3_free(err);
    throw std::runtime_error("initSchema failed: " + msg);
  }
}

/* ============================================================
 * Load SUD Model (Diagram)
 * ============================================================ */

SudModel SqliteStore::loadSudModel() const
{
  SudModel model;
  sqlite3* db = reinterpret_cast<sqlite3*>(db_);

  /* ---- load functions ---- */
  {
    const char* sql =
      "SELECT usr, name, file FROM sud_function;";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      SudFunction f;
      f.usr  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      f.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      f.file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
      model.functions.push_back(std::move(f));
    }

    sqlite3_finalize(stmt);
  }

  /* ---- load calls ---- */
  {
    const char* sql =
      "SELECT caller_usr, callee_usr FROM sud_call;";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      SudCall c;
      c.callerUSR = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      c.calleeUSR = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      model.calls.push_back(std::move(c));
    }

    sqlite3_finalize(stmt);
  }

  return model;
}

void SqliteStore::insertFunctions(const std::vector<SudFunction>& funcs)
{
  sqlite3* db = reinterpret_cast<sqlite3*>(db_);

  const char* sql =
    "INSERT OR IGNORE INTO sud_function (usr, name, file) VALUES (?, ?, ?);";

  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

  for (const auto& f : funcs) {
    sqlite3_bind_text(stmt, 1, f.usr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, f.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, f.file.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
}

void SqliteStore::insertCalls(const std::vector<SudCall>& calls)
{
  sqlite3* db = reinterpret_cast<sqlite3*>(db_);

  const char* sql =
    "INSERT INTO sud_call (caller_usr, callee_usr) VALUES (?, ?);";

  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

  for (const auto& c : calls) {
    sqlite3_bind_text(stmt, 1, c.callerUSR.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, c.calleeUSR.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
}


