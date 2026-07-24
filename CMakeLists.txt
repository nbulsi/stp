##cmake .. -DSTP_ENABLE_CUDA=ON

cmake_minimum_required(VERSION 3.19)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(POLICY CMP0148)
    set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
endif()

# Set C++ standard
set(STP_CXX_STANDARD "17" CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD ${STP_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(STP_ENABLE_CUDA "Enable CUDA acceleration backend" OFF)
option(STP_ENABLE_EIGEN "Enable the Eigen reference backend" OFF)

# Handle CUDA support
if(STP_ENABLE_CUDA)
    enable_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        project(stp LANGUAGES CXX CUDA)    
    else()
        message(FATAL_ERROR "CUDA is enabled, but CUDA toolkit was not found.")
    endif()
else()
    project(stp LANGUAGES CXX)
endif()

option(TEST "Build tests" OFF)

# Add public include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

add_compile_options(-g -O3)


# Build tests if enabled
if(TEST)
    add_subdirectory(test)
endif()

add_subdirectory(lib)
add_subdirectory(src)






