
set(SOURCES
        src/first_page.cc
        src/node_page.cc
        src/over_free_page.cc
)

set(HEADERS
        include/first_page.h
        include/node_page.h
        include/over_free_page.h
)

# Add library
add_library(DerivedPage ${SOURCES} ${HEADERS})
target_link_libraries(DerivedPage PRIVATE Utility)
target_link_libraries(DerivedPage PUBLIC Pager)

# Add gtest to enable FRIEND_TEST
target_link_libraries(DerivedPage PUBLIC gtest)

# Include directory for library
target_include_directories(
        DerivedPage
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utility/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pager/include
)


# Add subdirectory for tests
add_subdirectory(test)