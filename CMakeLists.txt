cmake_minimum_required (VERSION 2.8.12)
project (emptool)
set(NAME "emp-tool")

add_executable(emp_utils emp_utils.cpp)
target_include_directories(emp_utils PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(emp_utils PRIVATE aio ssl crypto gmp)
target_compile_options(emp_utils PRIVATE -pthread -march=native -maes -mrdseed -Wall -funroll-loops -Wno-ignored-attributes -Wno-unused-result -Wno-deprecated-declarations)
target_compile_features(emp_utils PRIVATE cxx_std_20)

add_executable(example_input example_input.cpp)
target_compile_options(example_input PRIVATE -Wall)
target_compile_features(example_input PRIVATE cxx_std_20)

install(TARGETS emp_utils example_input RUNTIME DESTINATION .)
