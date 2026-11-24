cmake_minimum_required(VERSION 3.19)

# Compatibility with the bundled third-party CMake projects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(POLICY CMP0148)
    set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
endif()

project(stp LANGUAGES CXX)

set(STP_CXX_STANDARD "17" CACHE STRING "C++ standard for STP targets")
set_property(CACHE STP_CXX_STANDARD PROPERTY STRINGS 17 20 23)
if(NOT STP_CXX_STANDARD MATCHES "^(17|20|23)$")
    message(FATAL_ERROR "STP_CXX_STANDARD must be 17, 20, or 23")
endif()
option(STP_ENABLE_CUDA "Enable CUDA acceleration backend" OFF)

if(STP_ENABLE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER)
        message(FATAL_ERROR "STP_ENABLE_CUDA=ON requires a CUDA Toolkit with nvcc; set CMAKE_CUDA_COMPILER if it is not on PATH")
    endif()
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
endif()

find_package(Threads REQUIRED)
include(CTest)

# Header-only algorithms and their public requirements.
add_library(stp_lib INTERFACE)
target_include_directories(stp_lib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(stp_lib INTERFACE cxx_std_${STP_CXX_STANDARD})
target_link_libraries(stp_lib INTERFACE Threads::Threads)

add_subdirectory(lib)
add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
