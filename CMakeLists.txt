cmake_minimum_required(VERSION 3.10)
project(hz-monitor)

set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3)
endif()

find_library(NVML_LIB NAMES nvidia-ml REQUIRED)

add_executable(${PROJECT_NAME}
    main.cc
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} ${NVML_LIB} pthread)

# === Install target ===
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
