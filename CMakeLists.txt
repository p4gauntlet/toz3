# Makefile for the toZ3

# sources for toZ3
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

set (TOZ3V2_COMMON_SRCS
    common/z3_interpreter.cpp
    common/operands.cpp
    common/expressions.cpp
    common/complex_type.cpp
    common/state.cpp
    common/type_map.cpp
    )

set (TOZ3V2_COMMON_HDRS
    common/z3_interpreter.h
    common/scope.h
    common/complex_type.h
    common/state.h
    common/type_map.h
    common/z3_int.h
    )

set (TOZ3V2_INTERPRET_SRCS
    interpret/main.cpp
    interpret/options.cpp
    )

set (TOZ3V2_INTERPRET_HDRS
    interpret/options.h
    )

set (TOZ3V2_COMPARE_SRCS
    compare/options.cpp
    compare/main.cpp
    )

set (TOZ3V2_COMPARE_HDRS
    compare/options.h
    )

build_unified(TOZ3V2_COMMON_SRCS)
add_library(p4toz3lib ${TOZ3V2_COMMON_SRCS})
target_link_libraries (p4toz3lib ${P4C_LIBRARIES} ${P4C_LIB_DEPS} -lz3)
add_dependencies(p4toz3lib genIR frontend)


build_unified(TOZ3V2_INTERPRET_SRCS)
add_executable(p4-to-z3 ${TOZ3V2_INTERPRET_SRCS})
target_link_libraries (p4-to-z3 p4toz3lib)
install(TARGETS p4-to-z3 RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

build_unified(TOZ3V2_COMPARE_SRCS)
add_executable(p4-check-equality ${TOZ3V2_COMPARE_SRCS})
target_link_libraries (p4-check-equality p4toz3lib)
install(TARGETS p4-check-equality RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})


add_custom_target(linkp4toz3_v2
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4-to-z3 ${P4C_BINARY_DIR}/p4-to-z3
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4-check-equality ${P4C_BINARY_DIR}/p4-check-equality
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4include ${CMAKE_CURRENT_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4_14include ${CMAKE_CURRENT_BINARY_DIR}/p4_14include
)

add_dependencies(p4c_driver linkp4toz3_v2)

