# Enables testing for current directory and below
enable_testing()

# Add a testing executable
add_executable(
        os_test
        os_test.cc
)

# Link the testing executable with the library
target_link_libraries(
        os_test
        OS
        GTest::gtest_main
)

# Add the test to Google Test
include(GoogleTest)
gtest_discover_tests(os_test)
