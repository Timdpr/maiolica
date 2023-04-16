cmake_minimum_required(VERSION 3.15)
project(maiolica)

set(CMAKE_CXX_STANDARD 20)

# Compiler options:
# -O3 = agressive optimisation https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
# -mdyn... = https://bugzilla.mozilla.org/show_bug.cgi?id=165851
# -flto = link time optimisation https://gcc.gnu.org/wiki/LinkTimeOptimization

if(MINGW)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -static-libstdc++ -static-libgcc")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -static-libstdc++ -static-libgcc -flto")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mdynamic-no-pic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -mdynamic-no-pic -flto")
endif()

# Enable sanitisers when in debug:
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize-address-use-after-return=always -fno-omit-frame-pointer")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined -fsanitize=integer -fsanitize=float-divide-by-zero -fsanitize=implicit-conversion -fsanitize=bounds -fsanitize=nullability")

message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")

add_executable(maiolica
        src/main.cpp src/defs.h src/data.cpp src/init.cpp
        src/io.cpp src/uci.cpp
        src/board.cpp src/bitboards.cpp src/hashkeys.cpp
        src/attack.cpp src/movegen.cpp src/validate.cpp src/makemove.cpp src/misc.cpp src/search.cpp src/pvtable.cpp src/evaluate.cpp
        src/perft.cpp
        )
target_compile_options(maiolica PRIVATE -Wall -Wextra -Wno-unused-parameter $<$<CONFIG:DEBUG>:-DDEBUG>)
