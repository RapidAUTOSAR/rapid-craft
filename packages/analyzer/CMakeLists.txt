project(rapid_craft_analyzer LANGUAGES C CXX)

# --------------------------------------------------
# Compiler options
# --------------------------------------------------
add_compile_options(-Wno-macro-redefined)

# --------------------------------------------------
# LLVM / Clang
# --------------------------------------------------
set(LLVM_USE_STATIC_LIBS OFF)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
# message(STATUS "LLVM libs: ${LLVM_AVAILABLE_LIBS}")
message(STATUS "LLVM_DIR: ${LLVM_DIR}")
message(STATUS "Clang_DIR: ${Clang_DIR}")

# --------------------------------------------------
# Target
# --------------------------------------------------
add_executable(rapid-craft-analyzer
  src/main.cpp
  src/CallGraphCollector.cpp
)

target_compile_features(rapid-craft-analyzer PRIVATE cxx_std_17)

target_compile_options(rapid-craft-analyzer PRIVATE
  -Wa,-mbig-obj
)

# LLVM 필수 매크로 (정상 형태로 직접 지정)
target_compile_definitions(rapid-craft-analyzer PRIVATE
  _FILE_OFFSET_BITS=64
  __STDC_CONSTANT_MACROS
  __STDC_FORMAT_MACROS
  __STDC_LIMIT_MACROS
)

# include dirs (target-scoped only)
target_include_directories(rapid-craft-analyzer PRIVATE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

llvm_map_components_to_libnames(LLVM_LIBS
  support
  core
  option
  demangle
  bitreader
  bitwriter
  mc
  mcparser
  mcdisassembler
  object
  irreader
  profiledata
  target
)

# --------------------------------------------------
# 🔑 핵심: monolithic LLVM/Clang DLL 링크
# --------------------------------------------------
target_link_libraries(rapid-craft-analyzer PRIVATE
  clang-cpp
  ${LLVM_LIBS}
)

target_link_options(rapid-craft-analyzer PRIVATE
  -fuse-ld=lld
)

# --------------------------------------------------
# Post build: Electron resource copy
# --------------------------------------------------
add_custom_command(TARGET rapid-craft-analyzer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_SOURCE_DIR}/packages/analyzer
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:rapid-craft-analyzer>
    ${CMAKE_SOURCE_DIR}/packages/analyzer/rapid-craft-analyzer.exe
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_SOURCE_DIR}/apps/rapid-craft-ui/resources
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:rapid-craft-analyzer>
    ${CMAKE_SOURCE_DIR}/apps/rapid-craft-ui/resources/rapid-craft-analyzer.exe
)
