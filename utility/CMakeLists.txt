
set(SOURCES
        src/sql_rc.cc
        src/utility.cc
)

set(HEADERS
        include/sql_rc.h
        include/sql_int.h
        include/sql_limit.h
)

add_library(Utility ${SOURCES} ${HEADERS})

target_include_directories(
        Utility
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(test)