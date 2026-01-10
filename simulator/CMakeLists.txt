cmake_minimum_required(VERSION 3.10)
project(led_simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2 and SDL2_ttf
find_package(SDL2 REQUIRED)
find_library(SDL2_TTF_LIBRARY SDL2_ttf)
find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2)

# Copy .ino file to .cpp for proper C++ compilation
set(INO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../Bonnaroo.ino")
set(INO_CPP "${CMAKE_CURRENT_BINARY_DIR}/Bonnaroo.cpp")

# Copy at build time to pick up changes
add_custom_command(
    OUTPUT ${INO_CPP}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INO_SOURCE} ${INO_CPP}
    DEPENDS ${INO_SOURCE}
    COMMENT "Copying Bonnaroo.ino to Bonnaroo.cpp"
)

# Include directories - mocks MUST come first to override real headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIR}
)

# Source files
set(SOURCES
    main.cpp
    ${INO_CPP}
    ${CMAKE_CURRENT_SOURCE_DIR}/../FilenameFunctions.cpp
)

# Create executable
add_executable(led_simulator ${SOURCES})

# Link SDL2 and SDL2_ttf
target_link_libraries(led_simulator ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARY})

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
