cmake_minimum_required(VERSION 3.21)

project(universal-macro LANGUAGES CXX)

option(UMACRO_BUILD_TESTS "Build format tests" ON)

add_subdirectory(core)
add_subdirectory(cli)

if (UMACRO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
