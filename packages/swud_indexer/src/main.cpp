#include "extractor_clang.h"
#include "storage_sqlite.h"
#include <iostream>

static void usage() {
  std::cout <<
    "swud_indexer --src <file.c> --db <out.db> -- <clang-args...>\n"
    "example:\n"
    "  swud_indexer --src App.c --db swud.db -- -Iinclude -DUNIT_TEST\n"
    "then:\n"
    "  swud_indexer --export-puml callgraph.puml --db swud.db\n";
}

int main(int argc, char** argv) {
  try {
    std::string src;
    std::string db;
    std::string exportPuml;

    std::vector<std::string> clangArgs;
    bool passArgs = false;

    for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--") { passArgs = true; continue; }
      if (!passArgs) {
        if (a == "--src" && i + 1 < argc) { src = argv[++i]; continue; }
        if (a == "--db" && i + 1 < argc) { db = argv[++i]; continue; }
        if (a == "--export-puml" && i + 1 < argc) { exportPuml = argv[++i]; continue; }
        if (a == "--help" || a == "-h") { usage(); return 0; }
      } else {
        clangArgs.push_back(a);
      }
    }

    if (db.empty()) { usage(); return 2; }

    SQLiteStore store(db);
    store.initSchema();

    if (!exportPuml.empty()) {
      store.exportCallGraphPuml(exportPuml);
      std::cout << "Exported: " << exportPuml << "\n";
      return 0;
    }

    if (src.empty()) { usage(); return 2; }

    // 최소한의 기본 args (필요시 조정)
    // C 파일이면 -x c / C++이면 -x c++
    // 인코딩/표준도 필요할 수 있음.
    if (clangArgs.empty()) {
      clangArgs = {"-x", "c", "-std=c11"};
    }

    ClangExtractor ex;
    ClangTUInput in{src, clangArgs};
    auto tu = ex.parse(in);

    store.upsertTU(tu);
    std::cout << "Indexed TU: " << src << "\n";
    std::cout << "Functions: " << tu.functions.size() << ", Calls: " << tu.calls.size() << "\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
