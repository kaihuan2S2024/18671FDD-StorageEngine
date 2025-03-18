# Enables testing for current directory and below
enable_testing()

# Add a testing executable
add_executable(
        pager_test
        pager_test.cc
)

add_executable(
        pager_concurrency_test
        pager_concurrency_test.cc
)

# Link the testing executable with the library
target_link_libraries(
        pager_test
        Pager
        GTest::gtest_main
)

target_link_libraries(
        pager_concurrency_test
        Pager
        GTest::gtest_main
)

# Add the test to Google Test
include(GoogleTest)
gtest_discover_tests(pager_test)
gtest_discover_tests(pager_concurrency_test)