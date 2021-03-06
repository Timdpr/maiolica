cmake_minimum_required(VERSION 3.15)
project(maiolica)

set(CMAKE_CXX_STANDARD 14)

# Compiler options:
# -O3 = agressive optimisation https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html,
# -mdyn... = https://bugzilla.mozilla.org/show_bug.cgi?id=165851,
# -flto = link time optimisation https://gcc.gnu.org/wiki/LinkTimeOptimization

if(MINGW)
    set(CMAKE_CXX_FLAGS "-O3 -static-libstdc++ -static-libgcc  -flto") # fallback/default
    set(CMAKE_CXX_FLAGS_DEBUG "-O3 -static-libstdc++ -static-libgcc -flto")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -static-libstdc++ -static-libgcc -flto")
else()
    set(CMAKE_CXX_FLAGS "-O3 -mdynamic-no-pic -flto") # fallback/default
    set(CMAKE_CXX_FLAGS_DEBUG "-O3 -mdynamic-no-pic -flto")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -mdynamic-no-pic -flto")
endif()

add_executable(maiolica src/main.cpp src/defs.h src/init.cpp src/bitboards.cpp src/hashkeys.cpp src/board.cpp src/data.cpp src/attack.cpp src/io.cpp src/movegen.cpp src/validate.cpp src/makemove.cpp src/perft.cpp src/misc.cpp src/search.cpp src/pvtable.cpp src/evaluate.cpp src/uci.cpp src/xboard.cpp)