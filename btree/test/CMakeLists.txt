# Enables testing for current directory and below
enable_testing()

# Add testing executables
add_executable(
        btree_developer_test
        btree_developer_test.cc
)

add_executable(
        btree_student_test
        btree_student_test.cc
)

# Link the testing executable with the library
target_link_libraries(
        btree_developer_test
        Btree
        GTest::gtest_main
)

target_link_libraries(
        btree_student_test
        Btree
        GTest::gtest_main
)

# Add the test to Google Test
include(GoogleTest)
gtest_discover_tests(btree_developer_test)
gtest_discover_tests(btree_student_test)
