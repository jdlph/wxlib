add_executable(meta_enum_test meta_enum_test.cpp)

get_filename_component(PARENT_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)
set(INCLUDE_DIR "${PARENT_DIR}/doctest/doctest")
target_include_directories(meta_enum_test PRIVATE ${INCLUDE_DIR})