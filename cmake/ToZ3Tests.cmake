# Keep skips independent of P4C's XFAIL/WILL_FAIL handling.
# Exit code 10 is EXIT_SKIPPED, used for ignored validator crashes.
function(toz3_add_validation_tests tag driver tests xfails)
  p4c_add_tests("${tag}" "${driver}" "${tests}" "${xfails}" ${ARGN})
  get_property(registered_tests DIRECTORY PROPERTY TESTS)
  foreach(test IN LISTS registered_tests)
    if(test MATCHES "^${tag}/")
      set_tests_properties("${test}" PROPERTIES SKIP_RETURN_CODE 10)
    endif()
  endforeach()
endfunction()
