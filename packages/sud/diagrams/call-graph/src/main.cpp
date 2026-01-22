#include "storage/SqliteStore.h"
#include "puml/PumlWriter.h"
#include <iostream>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cout << "sud-call-graph <db> <out.puml>\n";
    return 1;
  }

  SqliteStore db(argv[1]);
  auto model = db.loadSudModel();

  PumlWriter p;
  p.begin();
  for (auto& c : model.calls)
    p.arrow(c.callerUSR, c.calleeUSR);
  p.end();
  p.save(argv[2]);
}
