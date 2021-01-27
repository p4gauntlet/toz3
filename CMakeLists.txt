# Makefile for the toZ3

# sources for toZ3
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

set (TOZ3V2_SRCS
    toz3Options.cpp
    codegen.cpp
    complex_type.cpp
    main.cpp
    state.cpp
    )

set (TOZ3V2_HDRS
    toz3Options.h
    codegen.h
    scope.h
    complex_type.h
    state.h
    )


# extensions are unpleasant with unified builds
build_unified(TOZ3V2_SRCS)


add_executable(p4toz3_v2 ${TOZ3V2_SRCS})
target_link_libraries (p4toz3_v2 ${P4C_LIBRARIES} ${P4C_LIB_DEPS} -lz3)
add_dependencies(p4toz3_v2 genIR frontend)


install(TARGETS p4toz3_v2 RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})

add_custom_target(linkp4toz3_v2
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/p4toz3_v2 ${P4C_BINARY_DIR}/p4toz3_v2
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4include ${CMAKE_CURRENT_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR}/p4_14include ${CMAKE_CURRENT_BINARY_DIR}/p4_14include
)

add_dependencies(p4c_driver linkp4toz3_v2)

