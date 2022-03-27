cmake_minimum_required (VERSION 3.21)

project (semimap)
add_executable(semimap_test semimap_test.cpp)
# Set C++ standard for all targets
set_property(TARGET semimap_test PROPERTY CXX_STANDARD 23)

get_filename_component(PARENT_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)

set(INCLUDE_DIR "${PARENT_DIR}/doctest/doctest")
target_include_directories(semimap_test PRIVATE ${INCLUDE_DIR})
