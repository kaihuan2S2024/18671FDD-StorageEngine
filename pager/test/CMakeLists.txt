# Enables testing for current directory and below
enable_testing()

# Add a testing executable
add_executable(
        pager_test
        pager_test.cc
)

# Link the testing executable with the library
target_link_libraries(
        pager_test
        Pager
        GTest::gtest_main
)

# Add the test to Google Test
include(GoogleTest)
gtest_discover_tests(pager_test)