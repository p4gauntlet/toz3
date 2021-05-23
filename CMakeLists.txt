# Makefile for the toZ3

# sources for toZ3
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

set (TOZ3V2_COMMON_SRCS
    common/create_z3.cpp
    common/state.cpp
    common/type_simple.cpp
    common/type_table.cpp
    common/type_complex.cpp
    common/visitor_interpret.cpp
    common/visitor_specialize.cpp
    common/parser.cpp
    common/expressions.cpp
    common/operands.cpp
    common/util.cpp
    )

set (TOZ3V2_COMMON_HDRS
    common/create_z3.h
    common/scope.h
    common/state.h
    common/type_base.h
    common/type_simple.h
    common/type_complex.h
    common/visitor_interpret.h
    common/visitor_specialize.h
    common/util.h
    )

set (TOZ3V2_INTERPRET_SRCS
    interpret/main.cpp
    interpret/options.cpp
    )
set (TOZ3V2_INTERPRET_HDRS
    interpret/options.h
    )

set (TOZ3V2_COMPARE_SRCS
    compare/compare.cpp
    compare/options.cpp
    compare/main.cpp
    )
set (TOZ3V2_COMPARE_HDRS
    compare/compare.h
    compare/options.h
    )

set (TOZ3V2_VALIDATE_SRCS
    compare/compare.cpp
    validate/options.cpp
    validate/main.cpp
    )
set (TOZ3V2_VALIDATE_HDRS
    validate/options.h
    )

find_package (Boost REQUIRED COMPONENTS filesystem)

build_unified(TOZ3V2_COMMON_SRCS)
add_library(p4toz3lib ${TOZ3V2_COMMON_SRCS})
# add the Z3 includes
target_include_directories(p4toz3lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/contrib/z3)
target_link_libraries (p4toz3lib ${P4C_LIBRARIES} ${P4C_LIB_DEPS}
                        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/z3/libz3.a )
add_dependencies(p4toz3lib genIR frontend)


build_unified(TOZ3V2_INTERPRET_SRCS)
add_executable(p4toz3 ${TOZ3V2_INTERPRET_SRCS})
target_link_libraries (p4toz3 p4toz3lib)
install(TARGETS p4toz3 RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

build_unified(TOZ3V2_COMPARE_SRCS)
add_executable(p4compare ${TOZ3V2_COMPARE_SRCS})
target_link_libraries (p4compare p4toz3lib)
install(TARGETS p4compare RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})



build_unified(TOZ3V2_VALIDATE_SRCS)
add_executable(p4validate ${TOZ3V2_VALIDATE_SRCS})
target_link_libraries (p4validate p4toz3lib -lboost_filesystem)
install(TARGETS p4validate RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_custom_target(linkp4toz3
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4toz3 ${P4C_BINARY_DIR}/p4toz3
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4compare ${P4C_BINARY_DIR}/p4compare
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4validate ${P4C_BINARY_DIR}/p4validate
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4include ${CMAKE_CURRENT_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4_14include ${CMAKE_CURRENT_BINARY_DIR}/p4_14include
)

add_dependencies(p4c_driver linkp4toz3)

# This also builds the pruner module
add_subdirectory (pruner)
