cmake_minimum_required (VERSION 2.8.12)
project (emptool)
set(NAME "emp-tool")

include(CheckCSourceRuns)

add_executable(emp_utils emp_utils.cpp)
target_include_directories(emp_utils PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(emp_utils PRIVATE aio ssl crypto gmp)
target_compile_options(emp_utils PRIVATE -pthread -march=native -maes -mrdseed -Ofast -Wall -funroll-loops -Wno-ignored-attributes -Wno-unused-result -Wno-deprecated-declarations)
target_compile_features(emp_utils PRIVATE cxx_std_20)

check_c_source_runs("#include <x86intrin.h>\nint main(){\nunsigned long long r;\n_rdseed64_step(&r);\nreturn 0;\n}\n" RDSEED_RUN_RESULT)
string(COMPARE EQUAL "${RDSEED_RUN_RESULT}" "1" RDSEED_RUN_SUCCESS)
IF(NOT ${RDSEED_RUN_SUCCESS})
	target_compile_definitions(emp_utils PRIVATE EMP_ENABLE_RDSEED)
	message("-- Source of Randomness: random_device")
ELSE(NOT ${RDSEED_RUN_SUCCESS})
	message("-- Source of Randomness: rdseed")
ENDIF(NOT ${RDSEED_RUN_SUCCESS})

add_executable(example_input example_input.cpp)
target_compile_options(example_input PRIVATE -Wall)
target_compile_features(example_input PRIVATE cxx_std_20)

install(TARGETS emp_utils example_input RUNTIME DESTINATION .)
