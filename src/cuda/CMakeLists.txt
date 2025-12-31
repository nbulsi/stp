
message(STATUS "CMAKE_CUDA_COMPILER: ${CMAKE_CUDA_COMPILER}")
message(STATUS "STP_ENABLE_CUDA: ${STP_ENABLE_CUDA}")

#set(CMAKE_CUDA_ARCHITECTURES 86)
set(CMAKE_CUDA_ARCHITECTURES "native")
message(STATUS "CUDA Architectures: ${CMAKE_CUDA_ARCHITECTURES}")

#Set CUDA compiler options to generate position-independent code
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Xcompiler -fPIC")

#Add CUDA source files to the project
# include directories are handled by root CMakeLists.txt

add_library(stp_cuda  STATIC stp_cuda.cu)
set_target_properties( stp_cuda
                       PROPERTIES CUDA_SEPARABLE_COMPILATION ON
                      )
target_compile_features(stp_cuda PUBLIC cxx_std_17)

target_link_libraries(stp stp_cuda)
