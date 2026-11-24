# Follow the compiler/toolchain default unless the user selects target GPUs.
set(STP_CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}" CACHE STRING
    "CUDA architectures to build for (for example: 80 or 80;86)")

add_library(stp_cuda STATIC stp_cuda.cu boolean_cuda.cu)
target_link_libraries(stp_cuda PRIVATE stp_lib PUBLIC CUDA::cudart)
set_target_properties(stp_cuda PROPERTIES
    CUDA_ARCHITECTURES "${STP_CUDA_ARCHITECTURES}"
    CUDA_STANDARD ${STP_CXX_STANDARD}
    CUDA_STANDARD_REQUIRED ON
    CUDA_EXTENSIONS OFF
    CUDA_SEPARABLE_COMPILATION ON
    POSITION_INDEPENDENT_CODE ON
)
message(STATUS "STP CUDA architectures: ${STP_CUDA_ARCHITECTURES}")
