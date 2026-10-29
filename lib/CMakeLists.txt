if (NOT TARGET fmt)
  add_library(fmt INTERFACE) # v6.3.0
  target_include_directories(fmt SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fmt)
  target_compile_definitions(fmt INTERFACE FMT_HEADER_ONLY)
endif()

if(NOT TARGET alice)
    add_subdirectory(alice)
endif()