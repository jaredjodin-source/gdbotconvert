cmake_minimum_required(VERSION 3.21)

project(universal-macro LANGUAGES CXX)

option(UMACRO_BUILD_CLI "Build the command-line converter" ON)
option(UMACRO_BUILD_TESTS "Build format tests" ON)
option(UMACRO_BUILD_WEB "Build the WebAssembly converter" OFF)

add_subdirectory(core)

if (UMACRO_BUILD_CLI)
    add_subdirectory(cli)
endif()

if (UMACRO_BUILD_WEB)
    add_subdirectory(web)
endif()

if (UMACRO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
