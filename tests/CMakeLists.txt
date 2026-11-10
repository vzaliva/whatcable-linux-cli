add_executable(whatcable-tests
    test_main.cpp
    FakeSysfs.cpp
    Fixtures.cpp
    test_pd_decoder.cpp
    test_identity.cpp
    test_power_delivery.cpp
    test_charging.cpp
    test_device_manager.cpp
    test_vendor_db.cpp
)
target_link_libraries(whatcable-tests PRIVATE whatcablecore)

# Core library: decoding, sysfs parsing and verdicts over fake sysfs trees.
add_test(NAME core COMMAND whatcable-tests)

# CLI end to end, on a fake sysfs tree written by the test binary.
set(WHATCABLE_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/fixture)
add_test(NAME cli_fixture_setup
         COMMAND whatcable-tests --write-fixture ${WHATCABLE_FIXTURE})
set_tests_properties(cli_fixture_setup PROPERTIES FIXTURES_SETUP cli_fixture)

add_test(NAME cli_text COMMAND whatcable-linux)
set_tests_properties(cli_text PROPERTIES
    FIXTURES_REQUIRED cli_fixture
    ENVIRONMENT "WHATCABLE_SYSFS_ROOT=${WHATCABLE_FIXTURE}"
    PASS_REGULAR_EXPRESSION "Plugged in at 45W, not charging")

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME cli_json
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/check_cli_json.py
                     $<TARGET_FILE:whatcable-linux> ${WHATCABLE_FIXTURE})
    set_tests_properties(cli_json PROPERTIES FIXTURES_REQUIRED cli_fixture)
else()
    message(STATUS "Python 3 not found: skipping the cli_json test")
endif()
