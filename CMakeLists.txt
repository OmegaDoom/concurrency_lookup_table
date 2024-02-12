cmake_minimum_required(VERSION 3.20)

include(GoogleTest)

project(concurrent_lookup_table LANGUAGES CXX )
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)
add_library(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/concurrent_lookup_table.h)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(tests ${TEST_SOURCES})
enable_testing()
gtest_discover_tests(tests)
target_link_libraries(tests gtest)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
