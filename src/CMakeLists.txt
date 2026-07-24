
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


if(STP_ENABLE_EIGEN)
  find_package(Eigen3 QUIET)
  if(NOT Eigen3_FOUND)
    message(FATAL_ERROR
      "STP_ENABLE_EIGEN=ON requires Eigen3. Install Eigen3 or configure CMake with "
      "-DCMAKE_PREFIX_PATH=<eigen-install-prefix>.")
  endif()

  message(STATUS "Eigen reference backend enabled")
  target_link_libraries(stp_lib INTERFACE Eigen3::Eigen)
  target_compile_definitions(stp_lib INTERFACE STP_HAS_EIGEN=1)
endif()

target_link_libraries(stp PRIVATE fmt pthread stp_lib)
