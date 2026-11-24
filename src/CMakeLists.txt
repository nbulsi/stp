# The CLI and tests share the same simulation backend configuration.
add_library(stp_simulation INTERFACE)
target_link_libraries(stp_simulation INTERFACE stp_lib)

if(STP_ENABLE_CUDA)
    add_subdirectory(cuda)
    target_link_libraries(stp_simulation INTERFACE stp_cuda)
    target_compile_definitions(stp_simulation INTERFACE ENABLE_CUDA)
endif()

add_executable(stp stp.cpp)
target_link_libraries(stp PRIVATE stp_simulation alice fmt)
set_target_properties(stp PROPERTIES
    CXX_STANDARD ${STP_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
