
add_library(stp_lib INTERFACE)

# Include stp headers
target_include_directories(stp_lib SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(stp_lib SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/commands)

#set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SOURCES stp.cpp)
add_executable(stp ${SOURCES} ${FILENAMES})
target_link_libraries(stp PRIVATE alice)


if(STP_ENABLE_CUDA AND CMAKE_CUDA_COMPILER)
    add_subdirectory(cuda)
    target_compile_definitions(stp PUBLIC ENABLE_CUDA)
endif()


# Try to find Eigen3 on the system
find_package(Eigen3 QUIET)
message(STATUS "Eigen version: ${EIGEN3_VERSION}")

# If Eigen3 is not found, use ExternalProject to clone and build Eigen3
if(NOT Eigen3_FOUND)
  message("-- Eigen3 is not found. Please clone and build from GitLab...")
  message("git clone https://gitlab.com/libeigen/eigen.git")
  message("cd eigen")
  message("mkdir build")
  message("cd build")
  message("cmake ..")
  message("make install")
  message(FATAL_ERROR "Please visit https://eigen.tuxfamily.org/ for more details.")
else()
message("found")
message(STATUS "Eigen3 found at: ${EIGEN3_INCLUDE_DIRS}")
 # Add Eigen3 include directory to stp target
  target_include_directories(stp_lib INTERFACE ${EIGEN3_INCLUDE_DIRS})
endif()

target_link_libraries(stp PRIVATE fmt Eigen3::Eigen pthread stp_lib)
