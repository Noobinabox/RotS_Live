#usage : cmake -S . -B /../build
# this will create a build solution in the build directory
# .gitignore exludes everything under /build.

cmake_minimum_required(VERSION 3.10)
project(RotS)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS ".h")

add_executable(ageland ${SOURCES} ${HEADERS})

set_target_properties(ageland PROPERTIES CXX_STANDARD 17)

# Add debug flags
target_compile_options(ageland PRIVATE 
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# Enable debug symbols for all configurations
target_compile_options(ageland PRIVATE -g)