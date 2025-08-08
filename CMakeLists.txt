cmake_minimum_required(VERSION 3.10)
project(hz-monitor)

set(CMAKE_CXX_STANDARD 17)

find_library(NVML_LIB NAMES nvidia-ml REQUIRED)

add_executable(${PROJECT_NAME}
    main.cc
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} ${NVML_LIB})

# === Install target ===
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
