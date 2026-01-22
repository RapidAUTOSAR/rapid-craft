add_executable(sud-indexer
  src/main.cpp
  src/extractor_clang.cpp
)

target_link_libraries(sud-indexer
  PRIVATE rapid_common clang
)
