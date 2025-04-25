cmake_minimum_required (VERSION 2.8.12)
project (emptool)
set(NAME "emp-tool")

include(CheckCSourceRuns)

add_executable(emp_utils_garbler emp_utils.cpp)
target_compile_definitions(emp_utils_garbler PRIVATE PARTY=1)
add_executable(emp_utils_evaluator emp_utils.cpp)
target_compile_definitions(emp_utils_evaluator PRIVATE PARTY=2)

foreach(target emp_utils_garbler emp_utils_evaluator)
	target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR})
	target_link_libraries(${target} PRIVATE ssl crypto)
	target_compile_options(${target} PRIVATE -pthread -march=native -maes -mrdseed -Ofast -Wall -funroll-loops -Wno-ignored-attributes -Wno-unused-result -Wno-deprecated-declarations)
	target_compile_features(${target} PRIVATE cxx_std_20)
endforeach()

check_c_source_runs("#include <x86intrin.h>\nint main(){\nunsigned long long r;\n_rdseed64_step(&r);\nreturn 0;\n}\n" RDSEED_RUN_RESULT)
string(COMPARE EQUAL "${RDSEED_RUN_RESULT}" "1" RDSEED_RUN_SUCCESS)
IF(NOT ${RDSEED_RUN_SUCCESS})
	target_compile_definitions(emp_utils_garbler PRIVATE EMP_ENABLE_RDSEED)
	target_compile_definitions(emp_utils_evaluator PRIVATE EMP_ENABLE_RDSEED)
	message("-- Source of Randomness: random_device")
ELSE(NOT ${RDSEED_RUN_SUCCESS})
	message("-- Source of Randomness: rdseed")
ENDIF(NOT ${RDSEED_RUN_SUCCESS})

add_executable(example_input example_input.cpp)
target_compile_options(example_input PRIVATE -Wall)
target_compile_features(example_input PRIVATE cxx_std_20)

install(TARGETS emp_utils_garbler emp_utils_evaluator example_input RUNTIME DESTINATION .)
