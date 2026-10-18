configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY
)

# Handle the Z3 installation with this macro. Users have the option to supply their own Z3.
include(${P4C_SOURCE_DIR}/cmake/Z3.cmake)
obtain_z3()

OPTION (ENABLE_GAUNTLET_PRUNER "Build the Gauntlet Pruner." OFF)

set(
  TOZ3_COMMON_SRCS
  common/create_z3.cpp
  common/defaults.cpp
  common/type_inference.cpp
  common/state.cpp
  common/type_simple.cpp
  common/type_table.cpp
  common/type_complex.cpp
  common/visitor_interpret.cpp
  common/visitor_specialize.cpp
  common/packet.cpp
  common/parser.cpp
  common/expressions.cpp
  common/loops.cpp
  common/operands.cpp
  common/util.cpp
)

file(
  GLOB_RECURSE TOZ3_LINT_LIST FOLLOW_SYMLINKS
  common/*.cpp
  common/*.h
  compare/*.cpp
  compare/*.h
  interpret/*.cpp
  interpret/*.h
  pruner/*.cpp
  pruner/*.h
  validate/*.cpp
  validate/*.h
  tests/gtest/*.cpp
  tests/gtest/*.h
)
# Filter some folders from the CI checks.
list(FILTER TOZ3_LINT_LIST EXCLUDE REGEX "pruner/src/contrib")
add_clang_format_files(${P4C_SOURCE_DIR} "${TOZ3_LINT_LIST}")
add_cpplint_files(${P4C_SOURCE_DIR} "${TOZ3_LINT_LIST}")

# Enable clang-tidy for this extension and its subdirectories.
option(CMAKE_RUN_CLANG_TIDY "Run clang-tidy with the compiler." OFF)

if(CMAKE_RUN_CLANG_TIDY)
  if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "CMAKE_RUN_CLANG_TIDY requires an out-of-source build!")
  endif()
  find_program(TOZ3_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
  execute_process(
    COMMAND "${TOZ3_CLANG_TIDY_EXECUTABLE}" --version
    OUTPUT_VARIABLE clang_tidy_ver
    RESULT_VARIABLE clang_tidy_status
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT clang_tidy_status EQUAL 0)
    message(FATAL_ERROR "Unable to run ${TOZ3_CLANG_TIDY_EXECUTABLE} --version")
  endif()
  message(STATUS "Enabling ToZ3 clang-tidy checks: ${clang_tidy_ver}")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  # Unity sources live in the build tree and include our .cpp files as headers.
  # Lint each source directly, with our configuration, rather than P4C's.
  # These settings are scoped to ToZ3; P4C can still use unity builds.
  set(CMAKE_UNITY_BUILD OFF)
  set(TOZ3_CLANG_TIDY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy")
  set(CMAKE_CXX_CLANG_TIDY
    "${TOZ3_CLANG_TIDY_EXECUTABLE};--config-file=${TOZ3_CLANG_TIDY_CONFIG}")
  # Reconfigure and recheck sources when the lint policy changes, including
  # when their object files would otherwise be reused from a previous build.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TOZ3_CLANG_TIDY_CONFIG}")
  file(SHA1 "${TOZ3_CLANG_TIDY_CONFIG}" clang_tidy_sha1)
  add_compile_definitions("TOZ3_CLANG_TIDY_SHA1=${clang_tidy_sha1}")
endif()

add_library(p4toz3lib ${TOZ3_COMMON_SRCS})
# add the Z3 includes
target_include_directories(p4toz3lib SYSTEM BEFORE PUBLIC ${Z3_INCLUDE_DIR})
target_link_libraries(
  p4toz3lib PUBLIC ${P4C_LIBRARIES} ${P4C_LIB_DEPS} ${Z3_LIB}
)
add_dependencies(p4toz3lib genIR frontend)


add_executable(p4toz3 interpret/main.cpp interpret/options.cpp)
target_link_libraries(p4toz3 p4toz3lib)
install(TARGETS p4toz3 RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_executable(p4compare compare/main.cpp compare/options.cpp compare/compare.cpp)
target_link_libraries(p4compare p4toz3lib)
install(TARGETS p4compare RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_executable(p4validate validate/main.cpp validate/options.cpp validate/passes.cpp compare/compare.cpp)
target_link_libraries(p4validate p4toz3lib)
install(TARGETS p4validate RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

if(ENABLE_GTESTS)
  set(TOZ3_UNIT_TEST_SRCS
    tests/gtest/type_simple.cpp
    tests/gtest/type_inference.cpp
    tests/gtest/util.cpp
    tests/gtest/passes.cpp
    tests/gtest/loops.cpp
    tests/gtest/enums.cpp
    tests/gtest/slices.cpp
    tests/gtest/visitor.cpp
    tests/gtest/receivers.cpp
    tests/gtest/header_unions.cpp
    tests/gtest/parser.cpp
    tests/gtest/compare.cpp
  )
  add_executable(toz3_tests ${TOZ3_UNIT_TEST_SRCS} compare/compare.cpp validate/passes.cpp)
  target_link_libraries(toz3_tests PRIVATE p4toz3lib gtest_main)
  add_test(NAME toz3-unit COMMAND toz3_tests)
  set_tests_properties(toz3-unit PROPERTIES LABELS "gtest;toz3")
endif()


add_custom_target(
  linkp4toz3
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4toz3 ${P4C_BINARY_DIR}/p4toz3
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4compare ${P4C_BINARY_DIR}/p4compare
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4validate ${P4C_BINARY_DIR}/p4validate
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4include ${CMAKE_CURRENT_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4_14include ${CMAKE_CURRENT_BINARY_DIR}/p4_14include
)

add_dependencies(p4c_driver linkp4toz3)

# Test configuration
set(P4C_TEST_DIR "${CMAKE_SOURCE_DIR}/testdata/p4_16_samples")
set(TOZ3_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(VALIDATION_BIN "${CMAKE_BINARY_DIR}/p4validate")
set(COMPARE_BIN "${CMAKE_BINARY_DIR}/p4compare")
set(COMPILER_BIN "${CMAKE_BINARY_DIR}/p4test")
set(VALIDATION_DRIVER "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_validation_test.py")


################# P4C TESTS #################

file(GLOB P4C_VALIDATION_TESTS "${P4C_TEST_DIR}/*.p4")

# These tests need to be added manually

set(
  MANUAL_VALIDATION_TESTS
  ${P4C_TEST_DIR}/fabric_20190420/fabric.p4
)
set(P4C_VALIDATION_TESTS "${P4C_VALIDATION_TESTS};${MANUAL_VALIDATION_TESTS}")

set(
  P4C_VALIDATION_XFAIL_TESTS
  # SimplifyDefUse removes an assignment observable after extraction rejects.
  ${P4C_TEST_DIR}/invalid-hdr-warnings4.p4
  # SideEffectOrdering moves lookahead before a rejecting stack.next argument.
  ${P4C_TEST_DIR}/issue2090.p4
  ${P4C_TEST_DIR}/spec-ex19.p4
  # ParsersUnroll changes a short extraction's error at an explicit stack index.
  # See tests/violated/parser_index_short_packet (not a fallback-limit failure).
  ${P4C_TEST_DIR}/parser-unroll-test1.p4
  # Union validity mismatches. STF confirms field-write/extraction
  # defects; BMv2's invalidation behavior masks the copy/retained-type defects.
  ${P4C_TEST_DIR}/invalid-hdr-warnings5.p4
  ${P4C_TEST_DIR}/invalid-hdr-warnings6.p4
  ${P4C_TEST_DIR}/invalid-union.p4
  ${P4C_TEST_DIR}/issue561-4-bmv2.p4
  ${P4C_TEST_DIR}/issue561-5-bmv2.p4
  ${P4C_TEST_DIR}/issue561-6-bmv2.p4
  ${P4C_TEST_DIR}/pna-dpdk-header-union-stack.p4
  ${P4C_TEST_DIR}/pna-dpdk-header-union-stack1.p4
  ${P4C_TEST_DIR}/pna-dpdk-header-union-stack2.p4
  ${P4C_TEST_DIR}/pna-dpdk-invalid-hdr-warnings5.p4
  ${P4C_TEST_DIR}/pna-dpdk-invalid-hdr-warnings6.p4
  ${P4C_TEST_DIR}/union4-bmv2.p4
)

# CTest's WILL_FAIL does not invert a timeout. Keep these out of registration
# separately from XFAILs, which still run and must return a failure.
set(
  P4C_VALIDATION_TIMEOUT_TESTS
  # Interpreter: repeated nested control-loop unfolding consumes the time budget.
  ${P4C_TEST_DIR}/forloop3.p4
  ${P4C_TEST_DIR}/forloop4.p4
  # Interpreter: symbolic evaluation of the large pipeline has not reached comparison.
  ${P4C_TEST_DIR}/fabric_20190420/fabric.p4
  # Z3 comparison: both summaries are built; equality stalls between
  # FrontEnd_26_TableKeyNames and FrontEnd_27_PassRepeated.
  ${P4C_TEST_DIR}/issue561-bmv2.p4
  # Z3 comparison: symbolic popcount stalls between FrontEnd_35_MoveDeclarations
  # and FrontEnd_47_RemoveReturns, after summaries are built in under a second.
  ${P4C_TEST_DIR}/forloop2.p4
)
list(REMOVE_ITEM P4C_VALIDATION_TESTS ${P4C_VALIDATION_TIMEOUT_TESTS})

# Remaining programs need architecture-specific support or exceed validation limits.
set(
  SKIPPED_TESTS
  # Overloaded controls/packages and optional parameters on implemented controls
  # are outside the P4 specification's rules for these constructs.
  ${P4C_TEST_DIR}/issue1334.p4
  ${P4C_TEST_DIR}/issue3051.p4
  # A changing counter and modular conditional update exceed the iteration limit.
  ${P4C_TEST_DIR}/forloop10.p4
)

if(SKIPPED_TESTS)
  list(REMOVE_ITEM P4C_VALIDATION_TESTS ${SKIPPED_TESTS})
endif()


set(TEST_FLAGS "--compiler-bin ${COMPILER_BIN} --build-dir ${CMAKE_BINARY_DIR}")
if(VALIDATION_IGNORE_CRASHES)
  string(APPEND TEST_FLAGS " --ignore-crashes")
endif()
set(VALIDATION_FLAGS "--validation-bin ${VALIDATION_BIN} ${TEST_FLAGS}")
p4c_add_tests("toz3-validate-p4c" ${VALIDATION_DRIVER} "${P4C_VALIDATION_TESTS}" "${P4C_VALIDATION_XFAIL_TESTS}" "${VALIDATION_FLAGS}")

################# CUSTOM TESTS #################

file(GLOB VALIDATION_FRIENDS_TESTS "${TOZ3_TEST_DIR}/false_friends/*.p4")

p4c_add_tests("toz3-validate-friends" ${VALIDATION_DRIVER} "${VALIDATION_FRIENDS_TESTS}" "" "${VALIDATION_FLAGS}")

################# VIOLATION TESTS #################

file(GLOB VIOLATION_TESTS LIST_DIRECTORIES true "${TOZ3_TEST_DIR}/violated/*")

set(
  VIOLATION_XFAIL_TESTS
  # The loop branches on an uninitialized header field. With --allow-undefined,
  # comparison tolerates the mismatch, but this fixture requires a violation.
  extensions/toz3/tests/violated/parser_loop
)

set(VIOLATION_FLAGS "--validation-bin ${COMPARE_BIN} ${TEST_FLAGS} --check-violation")
p4c_add_tests("toz3-validate-violation" ${VALIDATION_DRIVER} "${VIOLATION_TESTS}" "${VIOLATION_XFAIL_TESTS}" "${VIOLATION_FLAGS}")

################# UNDEFINED TESTS #################

file(GLOB UNDEFINED_TESTS LIST_DIRECTORIES true "${TOZ3_TEST_DIR}/undef_violated/*")

set(
  UNDEFINED_XFAIL_TESTS
  # Invalid-header output masking makes these pairs equal even in strict mode;
  # the fixtures instead require a strict mismatch followed by undefined tolerance.
  extensions/toz3/tests/undef_violated/NestedStructs_1
  extensions/toz3/tests/undef_violated/NestedStructs_2
)

set(UNDEFINED_FLAGS "--validation-bin ${COMPARE_BIN} ${TEST_FLAGS} --check-undefined")
p4c_add_tests("toz3-validate-undefined" ${VALIDATION_DRIVER} "${UNDEFINED_TESTS}" "${UNDEFINED_XFAIL_TESTS}" "${UNDEFINED_FLAGS}")

# This also builds the pruner module
if(ENABLE_GAUNTLET_PRUNER)
  add_subdirectory(pruner)
endif()
