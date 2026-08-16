cmake_minimum_required(VERSION 3.21)

project(umacro-core LANGUAGES CXX)

# Geode already provides CPM; when building the core standalone (tests) we
# fetch it ourselves.
if (NOT COMMAND CPMAddPackage)
    set(CPM_DOWNLOAD_VERSION 0.40.2)
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
    if (NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
        file(DOWNLOAD
            "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
            ${CPM_DOWNLOAD_LOCATION}
        )
    endif()
    include(${CPM_DOWNLOAD_LOCATION})
endif()

CPMAddPackage("gh:nlohmann/json@3.11.3")
CPMAddPackage(
    NAME libGDR
    GITHUB_REPOSITORY maxnut/GDReplayFormat
    GIT_TAG 2a0ca3cff9b1d16863037a464da5fe27c28a5823
    DOWNLOAD_ONLY YES
)

add_library(umacro-core STATIC
    src/Registry.cpp
    src/Json.cpp
    src/Formats.cpp
    src/formats/Gdr.cpp
    src/formats/MegaHack.cpp
    src/formats/Tasbot.cpp
    src/formats/Echo.cpp
    src/formats/TextFormats.cpp
    src/formats/SimpleBinary.cpp
    src/formats/ReplayEngine.cpp
    src/formats/Silicate.cpp
)

target_include_directories(umacro-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(umacro-core SYSTEM PUBLIC ${libGDR_SOURCE_DIR}/include)

target_compile_features(umacro-core PUBLIC cxx_std_20)
target_link_libraries(umacro-core PUBLIC nlohmann_json::nlohmann_json)
set_target_properties(umacro-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
