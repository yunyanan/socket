# CMakeLists.txt

# reuqired version
cmake_minimum_required(VERSION 3.1)

# set the project name
project(socket)

# Set c standard
set(CMAKE_C_STANDARD 99)

# Enable the highest level warning
add_definitions(-Wall)

# set(CMAKE_C_FLAGS "-O0 -g")

# Add subdirectory
add_subdirectory(BlockTCP/)
add_subdirectory(SelectTCP/)
add_subdirectory(PollTCP/)
add_subdirectory(EpollTCP/)
