cmake_minimum_required(VERSION 3.25.2)
project(storageEngine)
set(CMAKE_CXX_STANDARD 17)

# Set compiler flags based on the operating system
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-generate")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
endif()

find_package(Boost 1.50 REQUIRED)
message(STATUS "Boost version: ${Boost_VERSION}")


# Add the main executable
add_executable(storageEngine main.cc)

# Include the subdirectories
add_subdirectory(utility)
add_subdirectory(pager)
add_subdirectory(os)
add_subdirectory(derived_page)
add_subdirectory(btree)


# Link the main executable with the submodule libraries
target_link_libraries(storageEngine PUBLIC gtest_main)
target_link_libraries(storageEngine PUBLIC Pager)
target_link_libraries(storageEngine PUBLIC Utility)
target_link_libraries(storageEngine PUBLIC OS)
target_link_libraries(storageEngine PUBLIC DerivedPage)
target_link_libraries(storageEngine PUBLIC Btree)

target_include_directories(storageEngine PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(storageEngine PRIVATE ${Boost_LIBRARIES})

add_test(NAME storageEngine_test COMMAND storageEngine)

## Include and link gtest
include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


#find_package(GTest QUIET)
#if(NOT GTest_FOUND)
#    include(FetchContent)
#    FetchContent_Declare(
#            googletest
#            URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
#    )
#    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#    FetchContent_MakeAvailable(googletest)
#endif()

## Add GTest required libraries to linker
link_libraries(${GTEST_LIBRARIES} gtest_main pthread)


add_executable(all_tests all_tests.cc)
# Link the new test executable with Google Test and your project libraries
target_link_libraries(all_tests
        GTest::gtest_main
        Utility
        OS
        Pager
        DerivedPage
        Btree
)
# Add the test to Google Test
include(GoogleTest)
gtest_discover_tests(all_tests)

