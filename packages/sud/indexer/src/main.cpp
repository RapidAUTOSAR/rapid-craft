#include <iostream>
#include <vector>
#include <string>

/* indexer only */
#include "extractor_clang.h"

/* common storage */
#include "storage/SqliteStore.h"

static void usage() {
  std::cout <<
    "sud-indexer --db <sud.db> [--src <file.c> ...] [--dir <path>] -- <clang-args>\n"
    "\n"
    "examples:\n"
    "  sud-indexer --db sud.db --src sample.c -- -std=c11 -Iinclude\n"
    "  sud-indexer --db sud.db --dir ./src -- -std=c11\n";
}

int main(int argc, char** argv) {
  std::string dbPath;
  std::vector<std::string> srcFiles;
  std::string srcDir;
  std::vector<std::string> clangArgs;

  bool passClangArgs = false;

  /* ------------------------------------------------------------
   * CLI parse
   * ------------------------------------------------------------ */
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];

    if (a == "--") {
      passClangArgs = true;
      continue;
    }

    if (!passClangArgs) {
      if (a == "--db" && i + 1 < argc) {
        dbPath = argv[++i];
        continue;
      }
      if (a == "--src" && i + 1 < argc) {
        srcFiles.emplace_back(argv[++i]);
        continue;
      }
      if (a == "--dir" && i + 1 < argc) {
        srcDir = argv[++i];
        continue;
      }
      if (a == "--help" || a == "-h") {
        usage();
        return 0;
      }
    } else {
      clangArgs.emplace_back(a);
    }
  }

  if (dbPath.empty() || (srcFiles.empty() && srcDir.empty())) {
    usage();
    return 1;
  }

  if (clangArgs.empty()) {
    clangArgs = { "-x", "c", "-std=c11" };
  }

  /* ------------------------------------------------------------
   * DB init
   * ------------------------------------------------------------ */
  SqliteStore store(dbPath);
  store.initSchema();

  ClangExtractor extractor;

  /* ------------------------------------------------------------
   * Index files
   * ------------------------------------------------------------ */
  auto indexOne = [&](const std::string& file) {
    ClangTUInput in{ file, clangArgs };
    IRTranslationUnit tu = extractor.parse(in);

    std::vector<SudFunction> funcs;
    funcs.reserve(tu.functions.size());
    for (const auto& f : tu.functions) {
      funcs.push_back(SudFunction{
        f.usr,
        f.name,
        f.filePath
      });
    }

    std::vector<SudCall> calls;
    calls.reserve(tu.calls.size());
    for (const auto& c : tu.calls) {
      calls.push_back(SudCall{
        c.callerUSR,
        c.calleeUSR
      });
    }

    store.insertFunctions(funcs);
    store.insertCalls(calls);

    std::cout << "[OK] " << file
              << " (functions=" << funcs.size()
              << ", calls=" << calls.size() << ")\n";
  };

  /* explicit file list */
  for (const auto& f : srcFiles) {
    try {
      indexOne(f);
    } catch (const std::exception& e) {
      std::cerr << "[FAIL] " << f << " : " << e.what() << "\n";
    }
  }

  /* ------------------------------------------------------------
   * NOTE:
   * Directory scan intentionally omitted here.
   * (Add later if needed — architecture is ready.)
   * ------------------------------------------------------------ */

  std::cout << "Indexing finished. DB = " << dbPath << "\n";
  return 0;
}
