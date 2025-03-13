set(SOURCES
	src/os.cc
	src/os_lock.cc)

set(HEADERS
	include/os.h
	)


# Add library
add_library(OS ${SOURCES} ${HEADERS})
target_link_libraries(OS PRIVATE Utility)


# Include directory for library
target_include_directories(OS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(
	OS
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/../utility/include
)

add_definitions(-DTHREADSAFE=1)
# Add subdirectory for tests
add_subdirectory(test)