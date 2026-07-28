
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

target_link_libraries(stp PRIVATE fmt pthread stp_lib)
