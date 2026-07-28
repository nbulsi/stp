add_executable(exprsim_tests test_case/exprsim_test.cpp)
target_include_directories(exprsim_tests PRIVATE ${PROJECT_SOURCE_DIR}/test/test_case)
target_link_libraries(exprsim_tests PRIVATE stp_lib)

add_test(NAME exprsim_tests COMMAND exprsim_tests)
