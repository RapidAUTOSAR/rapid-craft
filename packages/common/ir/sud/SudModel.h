#pragma once

#include <string>
#include <vector>

/*
 * ============================================================
 * Persistent SUD Model
 * - DB에 저장되는 "공식 모델"
 * - Diagram CLI가 사용하는 유일한 데이터 구조
 * ============================================================
 */

/* -------------------- Function -------------------- */
struct SudFunction {
  std::string usr;     // clang USR (unique id)
  std::string name;    // function name
  std::string file;    // source file path
};

/* -------------------- Call -------------------- */
struct SudCall {
  std::string callerUSR;
  std::string calleeUSR;
};

/* -------------------- Whole Model -------------------- */
struct SudModel {
  std::vector<SudFunction> functions;
  std::vector<SudCall> calls;
};
