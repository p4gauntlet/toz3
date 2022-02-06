include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestUtils.cmake)

set(
  P4PRUNER_SRCS
  src/main.cpp
  src/pruner_options.cpp
  src/pruner_util.cpp
  src/statement_pruner.cpp
  src/compiler_pruner.cpp
  src/expression_pruner.cpp
  src/boolean_pruner.cpp
  src/extended_unused.cpp
  src/replace_variables.cpp
  src/counter.cpp
)
set(
  P4PRUNER_HDRS
  src/pruner_options.h
  src/pruner_util.h
  src/statement_pruner.h
  src/compiler_pruner.h
  src/expression_pruner.h
  src/boolean_pruner.h
  src/extended_unused.h
  src/replace_variables.h
  src/counter.h
)

if(NOT CMAKE_RUN_CLANG_TIDY)
  # Only build unified if CMAKE_RUN_CLANG_TIDY is not enable.
  # clang-tidy does not work well with unified compilation
  build_unified(P4PRUNER_SRCS)
endif()

add_executable(p4pruner ${P4PRUNER_SRCS})
target_link_libraries(p4pruner ${P4C_LIBRARIES} ${P4C_LIB_DEPS})
add_dependencies(p4pruner frontend)


install(TARGETS p4pruner RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_custom_target(
  linkp4pruner
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4pruner ${P4C_BINARY_DIR}/p4pruner
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4include ${CMAKE_CURRENT_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4_14include ${CMAKE_CURRENT_BINARY_DIR}/p4_14include
)

add_dependencies(p4c_driver linkp4pruner)

# Test configuration
set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(P4C_TEST_BIN "${TEST_DIR}/broken_p4c/24895c1b28a35352f1d9ad1f43878c4f7061d3ab")
set(PRUNER_BIN "${CMAKE_CURRENT_BINARY_DIR}/p4pruner")
set(PRUNER_COMPILER_DRIVER "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_prog.py")

file(GLOB CRASH_TESTS "${TEST_DIR}/references/crash_bugs/*.p4")

# Remove references from the list of tests.
# TODO: Make this more systematic
file(GLOB CRASH_REFS "${TEST_DIR}/references/crash_bugs/*_reference.p4")
list(REMOVE_ITEM CRASH_TESTS ${CRASH_REFS})

p4c_add_tests("pruner" ${PRUNER_COMPILER_DRIVER} "${CRASH_TESTS}" "" "--pruner_path ${PRUNER_BIN} --compiler ${P4C_TEST_BIN} -ll DEBUG --type CRASH --p4prog")
