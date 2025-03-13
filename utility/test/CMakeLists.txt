# Enables testing for current directory and below
enable_testing()

## Add a testing executable
add_executable(
        sql_rc_test
        sql_rc_test.cc
)

# Link the testing executable with the library
target_link_libraries(
        sql_rc_test
        Utility
        GTest::gtest_main
)

# Add the test to Google Test
include(GoogleTest)
gtest_discover_tests(sql_rc_test)
