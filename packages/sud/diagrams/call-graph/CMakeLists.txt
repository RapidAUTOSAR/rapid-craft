add_executable(sud-call-graph
  src/main.cpp
)

target_link_libraries(sud-call-graph
  PRIVATE rapid_common
)
