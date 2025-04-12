
set(SOURCES
        src/btree.cc
        src/btree_bt_cursor_public.cc
        src/btree_bt_cursor_private.cc
        src/btree_balance.cc
)

set(HEADERS
        include/btree.h
)

# Add library
add_library(Btree ${SOURCES} ${HEADERS})
target_link_libraries(Btree PRIVATE Utility)
target_link_libraries(Btree PUBLIC Pager)
target_link_libraries(Btree PUBLIC DerivedPage)

# Include directory for library
target_include_directories(
        Btree
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utility/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pager/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../derived_page/include
)

# Add subdirectory for tests
add_subdirectory(test)