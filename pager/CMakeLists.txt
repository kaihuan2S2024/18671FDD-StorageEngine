
set(SOURCES
        src/pager.cc
        src/pager_cache.cc
        src/pager_journal.cc
)

set(HEADERS
        include/pager.h
)

set(Boost_USE_STATIC_LIBS OFF) # Only if needed by the inner library
set(Boost_USE_MULTITHREADED ON) # Only if needed by the inner library

find_package(Boost 1.83 REQUIRED)
message(STATUS "Boost version: ${Boost_VERSION}")


add_library(Pager ${SOURCES} ${HEADERS})
target_link_libraries(Pager PRIVATE Utility)
target_link_libraries(Pager PRIVATE OS)
target_include_directories(Pager PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(Pager PUBLIC ${Boost_LIBRARIES})


# Include directory for library
target_include_directories(
        Pager
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utility/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../os/include
)

# Add subdirectory for tests
add_subdirectory(test)