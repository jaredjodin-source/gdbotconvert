if (NOT EMSCRIPTEN)
    message(FATAL_ERROR "UMACRO_BUILD_WEB requires the Emscripten toolchain")
endif()

add_executable(umacro-web bindings.cpp)
target_link_libraries(umacro-web PRIVATE umacro-core)
target_compile_features(umacro-web PRIVATE cxx_std_20)

set_target_properties(umacro-web PROPERTIES
    OUTPUT_NAME converter
    SUFFIX .js
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

target_link_options(umacro-web PRIVATE
    -sALLOW_MEMORY_GROWTH=1
    -sENVIRONMENT=web
    -sFILESYSTEM=0
    -sMODULARIZE=1
    -sEXPORT_NAME=createUniversalMacro
    -sEXPORTED_FUNCTIONS=['_malloc','_free']
    -sEXPORTED_RUNTIME_METHODS=['ccall']
    -sNO_EXIT_RUNTIME=1
)
