# Enables testing for current directory and below
enable_testing()

# Add testing executables
add_executable(
        first_page_test
        first_page_test.cc
)

add_executable(
        over_free_test
        over_free_test.cc
)

# Link the testing executable with the library
target_link_libraries(
        first_page_test
        DerivedPage
        GTest::gtest_main
)


target_link_libraries(
        over_free_test
        DerivedPage
        GTest::gtest_main
)

# Add the test to Google Test
include(GoogleTest)
gtest_discover_tests(first_page_test)
gtest_discover_tests(over_free_test)
