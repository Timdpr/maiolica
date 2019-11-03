cmake_minimum_required(VERSION 3.15)
project(maiolica)

set(CMAKE_CXX_STANDARD 14)

add_executable(maiolica src/main.cpp src/defs.h src/init.cpp src/bitboards.cpp src/hashkeys.cpp src/board.cpp src/data.cpp src/attack.cpp)