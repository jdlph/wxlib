add_executable(semimap_test semimap_test.cpp)

get_filename_component(PARENT_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)
set(INCLUDE_DIR "${PARENT_DIR}/doctest/doctest")
target_include_directories(semimap_test PRIVATE ${INCLUDE_DIR})
