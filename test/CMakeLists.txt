add_executable(exprsim_tests
    test_case/exprsim_test.cpp
    test_case/lutsim_test.cpp
    test_case/dot_test.cpp
)
target_include_directories(exprsim_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test_case")
target_link_libraries(exprsim_tests PRIVATE stp_simulation)
target_compile_definitions(exprsim_tests PRIVATE STP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_target_properties(exprsim_tests PROPERTIES
    CXX_STANDARD ${STP_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# GPU runtime cases run separately, so CPU checks can run without a GPU.
add_test(NAME exprsim_tests COMMAND exprsim_tests "~[cuda]")
set_tests_properties(exprsim_tests PROPERTIES LABELS "cpu")
if(STP_ENABLE_CUDA)
    add_test(NAME cuda_runtime_tests COMMAND exprsim_tests "[cuda]")
    set_tests_properties(cuda_runtime_tests PROPERTIES LABELS "cuda")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME session_cli_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_case/session_cli_test.py" $<TARGET_FILE:stp>)
    add_test(NAME abc_truth_table_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_abc_benchmarks.py" --stp $<TARGET_FILE:stp>)
    set_tests_properties(session_cli_tests abc_truth_table_tests PROPERTIES LABELS "cpu")
else()
    message(STATUS "Python3 not found: skipping CLI session and ABC truth-table tests")
endif()
