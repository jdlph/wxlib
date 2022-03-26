cmake_minimum_required (VERSION 3.21)

project (semimap)
add_executable(semimap_test semimap_test.cpp)
# Set C++ standard for all targets
set_property(TARGET semimap_test PROPERTY CXX_STANDARD 23)
