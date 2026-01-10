cmake_minimum_required(VERSION 3.10)
project(led_simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 REQUIRED)

# Copy .ino file to .cpp for proper C++ compilation
set(INO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../Bonnaroo.ino")
set(INO_CPP "${CMAKE_CURRENT_BINARY_DIR}/Bonnaroo.cpp")

# Copy .ino to .cpp at configure time
configure_file(${INO_SOURCE} ${INO_CPP} COPYONLY)

# Include directories - mocks MUST come first to override real headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${SDL2_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    main.cpp
    ${INO_CPP}
    ${CMAKE_CURRENT_SOURCE_DIR}/../FilenameFunctions.cpp
)

# Create executable
add_executable(led_simulator ${SOURCES})

# Link SDL2
target_link_libraries(led_simulator ${SDL2_LIBRARIES})

# Compiler warnings
target_compile_options(led_simulator PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# Define that we're in simulator mode
target_compile_definitions(led_simulator PRIVATE SIMULATOR_MODE=1)

# Output to build directory
set_target_properties(led_simulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Print configuration
message(STATUS "SDL2 Include: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 Libraries: ${SDL2_LIBRARIES}")
