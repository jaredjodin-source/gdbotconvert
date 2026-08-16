add_executable(umacro main.cpp)
target_link_libraries(umacro PRIVATE umacro-core)
target_compile_features(umacro PRIVATE cxx_std_20)
