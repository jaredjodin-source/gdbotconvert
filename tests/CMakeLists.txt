add_executable(umacro-tests test_formats.cpp)
target_link_libraries(umacro-tests PRIVATE umacro-core)
target_compile_features(umacro-tests PRIVATE cxx_std_20)

add_test(NAME formats COMMAND umacro-tests)
