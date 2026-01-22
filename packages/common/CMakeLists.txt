add_library(rapid_common STATIC
  ir/sud/SudModel.h
  storage/SqliteStore.cpp
  puml/PumlWriter.cpp
)

target_include_directories(rapid_common PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(SQLite3 REQUIRED)
target_link_libraries(rapid_common PUBLIC SQLite::SQLite3)