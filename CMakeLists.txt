# Makefile for the toZ3

# sources for toZ3
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY
)

# Handle the Z3 installation with this macro. Users have the option to supply their own Z3.
include(${P4C_SOURCE_DIR}/cmake/Z3.cmake)
obtain_z3()

OPTION (ENABLE_GAUNTLET_PRUNER "Build the Gauntlet Pruner." OFF)

set(
  TOZ3V2_COMMON_SRCS
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

set(
  TOZ3V2_COMMON_HDRS
  common/create_z3.h
  common/evaluation_context.h
  common/defaults.h
  common/type_inference.h
  common/packet.h
  common/scope.h
  common/state.h
  common/type_base.h
  common/type_simple.h
  common/type_complex.h
  common/visitor_interpret.h
  common/visitor_specialize.h
  common/util.h
)

set(
  TOZ3V2_INTERPRET_SRCS
  interpret/main.cpp
  interpret/options.cpp
)
set(
  TOZ3V2_INTERPRET_HDRS
  interpret/options.h
)

set(
  TOZ3V2_COMPARE_SRCS
  compare/compare.cpp
  compare/options.cpp
  compare/main.cpp
)
set(
  TOZ3V2_COMPARE_HDRS
  compare/compare.h
  compare/options.h
)

set(
  TOZ3V2_VALIDATE_SRCS
  compare/compare.cpp
  validate/options.cpp
  validate/main.cpp
  validate/passes.cpp
)
set(
  TOZ3V2_VALIDATE_HDRS
  validate/options.h
  validate/passes.h
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

# Enable clang-tidy to force consistent code style across the project.
# See also: https://gitlab.kitware.com/cmake/cmake/-/issues/18926
option(CMAKE_RUN_CLANG_TIDY "Run clang-tidy with the compiler." OFF)

if(CMAKE_RUN_CLANG_TIDY)
  message(STATUS "Enabling clang-tidy checks.")
  message(STATUS "Exporting compile commands for clang-tidy.")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  if(CMake_SOURCE_DIR STREQUAL CMake_BINARY_DIR)
    message(FATAL_ERROR "CMAKE_RUN_CLANG_TIDY requires an out-of-source build!")
  endif()
  set(CLANG_TIDY_COMMAND "${PROJECT_SOURCE_DIR}/scripts/tools/clang-tidy")
  execute_process(
    COMMAND ${CLANG_TIDY_COMMAND} --version
    OUTPUT_VARIABLE clang_tidy_ver
  )
  message(STATUS "clang-tidy: ${clang_tidy_ver}")
  set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
  # Create a preprocessor definition that depends on .clang-tidy content so
  # the compile command will change when .clang-tidy changes.  This ensures
  # that a subsequent build re-runs clang-tidy on all sources even if they
  # do not otherwise need to be recompiled.  Nothing actually uses this
  # definition.  We add it to targets on which we run clang-tidy just to
  # get the build dependency on the .clang-tidy file.
  file(SHA1 ${CMAKE_CURRENT_LIST_DIR}/.clang-tidy clang_tidy_sha1)
  set(CLANG_TIDY_DEFINITIONS "CLANG_TIDY_SHA1=${clang_tidy_sha1}")
  unset(clang_tidy_sha1)
endif()

add_library(p4toz3lib ${TOZ3V2_COMMON_SRCS})
# add the Z3 includes
target_include_directories(p4toz3lib SYSTEM BEFORE PUBLIC ${Z3_INCLUDE_DIR})
target_link_libraries(
  p4toz3lib PUBLIC ${P4C_LIBRARIES} ${P4C_LIB_DEPS} ${Z3_LIB}
)
add_dependencies(p4toz3lib genIR frontend)


add_executable(p4toz3 ${TOZ3V2_INTERPRET_SRCS})
target_link_libraries(p4toz3 p4toz3lib)
install(TARGETS p4toz3 RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_executable(p4compare ${TOZ3V2_COMPARE_SRCS})
target_link_libraries(p4compare p4toz3lib)
install(TARGETS p4compare RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_executable(p4validate ${TOZ3V2_VALIDATE_SRCS})
target_link_libraries(p4validate p4toz3lib)
install(TARGETS p4validate RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

if(ENABLE_GTESTS)
  set(TOZ3_UNIT_TEST_SRCS
    tests/gtest/type_simple.cpp
    tests/gtest/util.cpp
    tests/gtest/passes.cpp
    tests/gtest/loops.cpp
    tests/gtest/enums.cpp
    tests/gtest/slices.cpp
    tests/gtest/visitor.cpp
    tests/gtest/receivers.cpp
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
  # Confirmed compiler defects at P4C 3d9a20e985; reduced witnesses are in
  # tests/violated/parser_short_extract_def_use and parser_stack_argument_order.
  # SimplifyDefUse removes an assignment observable after extraction rejects.
  ${P4C_TEST_DIR}/invalid-hdr-warnings4.p4
  # SideEffectOrdering moves lookahead before a rejecting stack.next argument.
  ${P4C_TEST_DIR}/issue2090.p4
  ${P4C_TEST_DIR}/spec-ex19.p4
  # Interpreter: construction/selection of arrays of extern instances is unsupported.
  ${P4C_TEST_DIR}/generic-extern-inference.p4
  # Interpreter: packet-consuming parser cycles exceed 1,000 unfolding rounds;
  # these cycles have no proved stack-progress bound and still need summaries.
  ${P4C_TEST_DIR}/gauntlet_infinite_loop.p4
  ${P4C_TEST_DIR}/issue2314.p4
  ${P4C_TEST_DIR}/parser-unroll-test1.p4
  # Interpreter: Z3 runs out of memory while constructing the first parser summary,
  # before comparison (observed with a 3 GiB process limit).
  ${P4C_TEST_DIR}/invalid-hdr-warnings1.p4
  # Compiler: p4test throws std::bad_alloc in LocalCopyPropagation while dumping
  # passes, before interpretation (observed with the same 3 GiB process limit).
  ${P4C_TEST_DIR}/switch_p4_16.p4
)

# CTest's WILL_FAIL does not invert a timeout. Keep these out of registration
# separately from XFAILs, which still run and must return a failure.
set(
  P4C_VALIDATION_TIMEOUT_TESTS
  # Interpreter: repeated nested control-loop unfolding consumes the time budget.
  ${P4C_TEST_DIR}/forloop3.p4
  ${P4C_TEST_DIR}/forloop4.p4
  # Interpreter: a packet-consuming VLAN loop repeatedly invokes subparsers and
  # builds growing summaries, before any pass comparison.
  ${P4C_TEST_DIR}/issue281.p4
  # Interpreter: symbolic evaluation of the large pipeline has not reached comparison.
  ${P4C_TEST_DIR}/fabric_20190420/fabric.p4
  # Interpreter: parser-loop unfolding builds growing path guards; this includes
  # Z3 reachability queries inside interpretation, rather than pass comparison.
  ${P4C_TEST_DIR}/lookahead_ubpf.p4
  ${P4C_TEST_DIR}/parser-unroll-issue3537.p4
  ${P4C_TEST_DIR}/parser-unroll-issue3537-1.p4
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
  # Arrays of extern objects need receiver selection and a suitable extern model.
  ${P4C_TEST_DIR}/externArray1.p4
  # A changing counter and modular conditional update exceed the iteration limit.
  ${P4C_TEST_DIR}/forloop10.p4
)

if(SKIPPED_TESTS)
  list(REMOVE_ITEM P4C_VALIDATION_TESTS ${SKIPPED_TESTS})
endif()


set(VALIDATION_FLAGS "--validation-bin ${VALIDATION_BIN} --compiler-bin ${COMPILER_BIN} --build-dir ${CMAKE_BINARY_DIR}")
if(VALIDATION_IGNORE_CRASHES)
  set(VALIDATION_FLAGS "${VALIDATION_FLAGS} --ignore-crashes")
endif()
p4c_add_tests("toz3-validate-p4c" ${VALIDATION_DRIVER} "${P4C_VALIDATION_TESTS}" "${P4C_VALIDATION_XFAIL_TESTS}" "${VALIDATION_FLAGS}")

################# CUSTOM TESTS #################

file(GLOB VALIDATION_FRIENDS_TESTS "${TOZ3_TEST_DIR}/false_friends/*.p4")

set(VALIDATION_FRIENDS_XFAIL_TESTS)

# These tests need to be remove because they have spurious behavior (timeouts).
set(SKIPPED_TESTS)
if(SKIPPED_TESTS)
  list(REMOVE_ITEM VALIDATION_FRIENDS_TESTS ${SKIPPED_TESTS})
endif()

set(VALIDATION_FRIENDS_FLAGS "--validation-bin ${VALIDATION_BIN} --compiler-bin ${COMPILER_BIN} --build-dir ${CMAKE_BINARY_DIR}")
if(VALIDATION_IGNORE_CRASHES)
  set(VALIDATION_FRIENDS_FLAGS "${VALIDATION_FRIENDS_FLAGS} --ignore-crashes")
endif()
p4c_add_tests("toz3-validate-friends" ${VALIDATION_DRIVER} "${VALIDATION_FRIENDS_TESTS}" "${P4C_VALIDATION_XFAIL_TESTS}" "${VALIDATION_FLAGS}")

################# VIOLATION TESTS #################

file(GLOB VIOLATION_TESTS LIST_DIRECTORIES true "${TOZ3_TEST_DIR}/violated/*")

set(
  VIOLATION_XFAIL_TESTS
  # Interpreter: the packet-consuming subparser loop exceeds 1,000 unfolding rounds.
  extensions/toz3/tests/violated/2314_regression
  # The loop branches on an uninitialized header field. With --allow-undefined,
  # comparison tolerates the mismatch, but this fixture requires a violation.
  extensions/toz3/tests/violated/parser_loop
)

set(VIOLATION_FLAGS "--validation-bin ${COMPARE_BIN} --compiler-bin ${COMPILER_BIN} --build-dir ${CMAKE_BINARY_DIR} --check-violation")
if(VALIDATION_IGNORE_CRASHES)
  set(VIOLATION_FLAGS "${VIOLATION_FLAGS} --ignore-crashes")
endif()
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

set(UNDEFINED_FLAGS "--validation-bin ${COMPARE_BIN} --compiler-bin ${COMPILER_BIN} --build-dir ${CMAKE_BINARY_DIR} --check-undefined")
if(VALIDATION_IGNORE_CRASHES)
  set(UNDEFINED_FLAGS "${UNDEFINED_FLAGS} --ignore-crashes")
endif()
p4c_add_tests("toz3-validate-undefined" ${VALIDATION_DRIVER} "${UNDEFINED_TESTS}" "${UNDEFINED_XFAIL_TESTS}" "${UNDEFINED_FLAGS}")

# This also builds the pruner module
if(ENABLE_GAUNTLET_PRUNER)
  add_subdirectory(pruner)
endif()
