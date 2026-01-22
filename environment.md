# Install cmake
https://cmake.org/download/

# Install MSYS2
https://www.msys2.org/

pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-llvm \
  mingw-w64-x86_64-clang \
  mingw-w64-x86_64-clang-tools-extra \
  mingw-w64-x86_64-zlib \
  mingw-w64-x86_64-libffi \
  mingw-w64-x86_64-lld \
  mingw-w64-x86_64-sqlite3

# vscode  
Remove-Item -Recurse -Force build
cmake -S . -B build -G Ninja `
  -DCMAKE_C_COMPILER=clang `
  -DCMAKE_CXX_COMPILER=clang++ `
  -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 `
  -DLLVM_DIR=C:/msys64/mingw64/lib/cmake/llvm `
  -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build

# test
packages\analyzer\rapid-craft-analyzer.exe `
   .\sample\some_test.c `
   --emit=puml `
   --stdlib-leaf=on `
   --indirect-label=var

build\packages\sud\indexer\sud-indexer.exe `
  --db sud.db `
  --src sample\some_test.c `
  -- -std=c17 -Iinclude

build\packages\sud\diagrams\call-graph\sud-call-graph.exe `
  --db sud.db `
  --out callgraph_main_d3.puml `
  --root main `
  --depth 3
