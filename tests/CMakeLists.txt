file(GLOB URBS_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(urbs_tests ${URBS_TEST_SOURCES})
target_link_libraries(urbs_tests PRIVATE urbs_sim doctest_lib urbs_alloc_hook)
target_compile_definitions(urbs_tests PRIVATE URBS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
urbs_warnings(urbs_tests)
add_test(NAME unit COMMAND urbs_tests)

# Headless scenario tests: every tests/scenarios/*.json with a "test" block is run for its ticks.
file(GLOB URBS_SCENARIOS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.json)
foreach(sc ${URBS_SCENARIOS})
  get_filename_component(_n ${sc} NAME_WE)
  # Long runs (stress, multi-year economy/generations) are labelled "long": ctest -L long (release builds).
  add_test(NAME scenario_${_n} COMMAND urbs_headless --scenario ${sc} --test)
  if(_n MATCHES "^(stress_|economy_|generations_)")
    set_tests_properties(scenario_${_n} PROPERTIES LABELS long)
  endif()
endforeach()
