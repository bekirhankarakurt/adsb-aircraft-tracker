add_executable(adsb_tests
  main.cpp
  test_crc.cpp
  test_message.cpp
  test_cpr.cpp
  test_demod.cpp
  test_input.cpp
  test_tracker.cpp
  test_output.cpp
  test_samples.cpp)
target_link_libraries(adsb_tests PRIVATE adsb)
target_compile_definitions(adsb_tests PRIVATE ADSB_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/samples")

foreach(suite crc message cpr demod input tracker output samples)
  add_test(NAME unit.${suite} COMMAND adsb_tests ${suite})
endforeach()

set(SAMPLES ${PROJECT_SOURCE_DIR}/samples)
add_test(NAME cli.iq COMMAND adsb-tracker ${SAMPLES}/sample.iq)
set_tests_properties(cli.iq PROPERTIES PASS_REGULAR_EXPRESSION
  "THY4KL.*TKJ77.*PGT2312.*[0-9]+ corrected")
add_test(NAME cli.hex COMMAND adsb-tracker ${SAMPLES}/reference.hex)
set_tests_properties(cli.hex PROPERTIES PASS_REGULAR_EXPRESSION
  "52\\.2658.*3\\.9389.*KLM1023")
add_test(NAME cli.stdin
  COMMAND ${CMAKE_COMMAND} -DTRACKER=$<TARGET_FILE:adsb-tracker> -DINPUT=${SAMPLES}/sample.iq
          -P ${CMAKE_CURRENT_SOURCE_DIR}/stdin_check.cmake)
add_test(NAME cli.bad_option COMMAND adsb-tracker --nope)
set_tests_properties(cli.bad_option PROPERTIES WILL_FAIL TRUE)
