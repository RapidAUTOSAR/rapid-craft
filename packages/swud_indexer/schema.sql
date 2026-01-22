-- schema.sql (코드에서 자동 생성해도 됨)
PRAGMA journal_mode=WAL;

CREATE TABLE IF NOT EXISTS file (
  id INTEGER PRIMARY KEY,
  path TEXT UNIQUE,
  hash TEXT
);

CREATE TABLE IF NOT EXISTS function (
  id INTEGER PRIMARY KEY,
  usr TEXT UNIQUE,          -- clang USR (stable-ish unique key)
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
  call_type TEXT,          -- direct/indirect/macro (Phase1은 direct만)
  FOREIGN KEY(file_id) REFERENCES file(id)
);

CREATE INDEX IF NOT EXISTS idx_fn_file ON function(file_id);
CREATE INDEX IF NOT EXISTS idx_call_caller ON function_call(caller_usr);
CREATE INDEX IF NOT EXISTS idx_call_callee ON function_call(callee_usr);
