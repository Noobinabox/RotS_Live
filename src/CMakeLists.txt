#usage : cmake -S . -B /../build
# this will create a build solution in the build directory
# .gitignore exludes everything under /build.

cmake_minimum_required(VERSION 3.10)
project(RotS)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS ".h")

add_executable(ageland ${SOURCES} ${HEADERS})

set_target_properties(ageland PROPERTIES CXX_STANDARD 17)