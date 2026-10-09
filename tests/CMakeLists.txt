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

# Campaign balance: the autoplay bot must win each finished mission (long; release builds).
file(GLOB URBS_MISSIONS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/data/scenarios/m*/scenario.json)
foreach(sc ${URBS_MISSIONS})
  get_filename_component(_dir ${sc} DIRECTORY)
  get_filename_component(_n ${_dir} NAME)
  add_test(NAME mission_${_n} COMMAND urbs_headless --scenario ${sc} --ticks 192000 --autoplay)
  set_tests_properties(mission_${_n} PROPERTIES LABELS long PASS_REGULAR_EXPRESSION "mission WON")
endforeach()

# Save compatibility: saves written by older releases must still load and run.
file(GLOB URBS_OLD_SAVES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/saves/*.urbs)
foreach(sv ${URBS_OLD_SAVES})
  get_filename_component(_n ${sv} NAME_WE)
  add_test(NAME save_${_n} COMMAND urbs_headless --load ${sv} --ticks 1200)
  set_tests_properties(save_${_n} PROPERTIES PASS_REGULAR_EXPRESSION "loaded .*alloc_violations\":0")
endforeach()
