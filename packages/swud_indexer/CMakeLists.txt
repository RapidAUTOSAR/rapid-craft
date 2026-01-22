project(swud_phase1 LANGUAGES C CXX)

add_executable(swud_indexer
  src/main.cpp
  src/extractor_clang.cpp
  src/storage_sqlite.cpp
)

target_include_directories(swud_indexer PRIVATE src)

# clang-c headers
# e.g. -IC:/msys64/mingw64/include
# sqlite headers too
# (필요하면 사용자가 -DCMAKE_INCLUDE_PATH로 넘겨도 됨)

find_package(SQLite3 REQUIRED)

# libclang (C API) linking:
# MSYS2 mingw64 기준: -lclang or libclang.dll.a
# 경로가 다르면 target_link_directories로 추가
target_link_libraries(swud_indexer PRIVATE SQLite::SQLite3 clang)

# --------------------------------------------------
# Post build: Electron resource copy
# --------------------------------------------------
add_custom_command(TARGET swud_indexer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_SOURCE_DIR}/packages/swud_indexer
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:swud_indexer>
    ${CMAKE_SOURCE_DIR}/packages/swud_indexer/swud_indexer.exe
)