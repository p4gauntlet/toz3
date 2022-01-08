
set (P4PRUNER_SRCS
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
set (P4PRUNER_HDRS
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
target_link_libraries (p4pruner ${P4C_LIBRARIES} ${P4C_LIB_DEPS})
add_dependencies(p4pruner frontend)


install(TARGETS p4pruner  RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_custom_target(linkp4pruner
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

message("CRASH_TESTS ${CRASH_TESTS}")


macro(p4c_add_test_with_args tag driver isXfail alias p4test test_args cmake_args)
  set(__testfile "${P4C_BINARY_DIR}/${tag}/${p4test}.test")
  file (WRITE  ${__testfile} "#! /usr/bin/env bash\n")
  file (APPEND ${__testfile} "# Generated file, modify with care\n\n")
  file (APPEND ${__testfile} "cd ${P4C_BINARY_DIR}\n")
  file (APPEND ${__testfile} "${driver} ${test_args} \"$@\" ${P4C_SOURCE_DIR}/${p4test}")
  execute_process(COMMAND chmod +x ${__testfile})
  p4c_test_set_name(__testname ${tag} ${alias})
  separate_arguments(__args UNIX_COMMAND ${cmake_args})
  add_test (NAME ${__testname}
    COMMAND ${tag}/${p4test}.test ${__args}
    WORKING_DIRECTORY ${P4C_BINARY_DIR})
  if (NOT DEFINED ${tag}_timeout)
    set (${tag}_timeout 300)
  endif()
  if (${isXfail})
    set_tests_properties(${__testname} PROPERTIES LABELS "${tag};XFAIL" TIMEOUT ${${tag}_timeout} WILL_FAIL 1)
  else()
    set_tests_properties(${__testname} PROPERTIES LABELS ${tag} TIMEOUT ${${tag}_timeout})
  endif()
endmacro(p4c_add_test_with_args)

macro(p4c_add_test_list tag driver tests xfail)
  set (__xfail_list "${xfail}")
  set (__test_list "${tests}")
  set (__testCounter 0)
  set (__xfailCounter 0)
  list (LENGTH __test_list __nTests)
  foreach(t ${__test_list})
    list (FIND __xfail_list ${t} __xfail_test)
    if(__xfail_test GREATER -1)
      p4c_add_test_with_args (${tag} ${driver} TRUE ${t} ${t} "${ARGN}" "")
      math (EXPR __xfailCounter "${__xfailCounter} + 1")
    else()
      p4c_add_test_with_args (${tag} ${driver} FALSE ${t} ${t} "${ARGN}" "")
    endif() # __xfail_test
  endforeach() # tests
  math (EXPR __testCounter "${__testCounter} + ${__nTests}")
  # add the tag to the list
  set (TEST_TAGS ${TEST_TAGS} ${tag} CACHE INTERNAL "test tags")
  MESSAGE(STATUS "Added ${__testCounter} tests to '${tag}' (${__xfailCounter} xfails)")
endmacro(p4c_add_test_list)

macro(p4c_add_tests tag driver testsuites xfails)
  set(__tests "")
  set(__xfails "")
  p4c_find_test_names("${testsuites}" __tests)
  p4c_find_test_names("${xfails}" __xfails)
  p4c_add_test_list (${tag} ${driver} "${__tests}" "${__xfails}" "${ARGN}")
endmacro(p4c_add_tests)


p4c_add_tests("pruner" ${PRUNER_COMPILER_DRIVER} "${CRASH_TESTS}" "" "--pruner_path ${PRUNER_BIN} --compiler ${P4C_TEST_BIN} -ll DEBUG --type CRASH --p4prog")
