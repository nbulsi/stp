add_executable(exprsim_tests test_case/exprsim_test.cpp test_case/lutsim_test.cpp)
target_include_directories(exprsim_tests PRIVATE ${PROJECT_SOURCE_DIR}/test/test_case)
target_link_libraries(exprsim_tests PRIVATE stp_lib)
target_compile_definitions(exprsim_tests PRIVATE STP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_test(NAME exprsim_tests COMMAND exprsim_tests)
