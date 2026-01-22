#include "storage/SqliteStore.h"
#include "puml/PumlWriter.h"
#include <iostream>

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cout << "sud-sequence-diagram <db> <function> <out.puml>\n";
    return 1;
  }

  std::string root = argv[2];
  SqliteStore db(argv[1]);
  auto model = db.loadSudModel();

  PumlWriter p;
  p.begin();
  for (auto& c : model.calls) {
    if (c.callerUSR == root)
      p.arrow(c.callerUSR, c.calleeUSR);
  }
  p.end();
  p.save(argv[3]);
}
