cmake_minimum_required(VERSION 3.21)
project(maiolica VERSION 1.0 LANGUAGES CXX)

# Default to an optimised build, since that's what you want for playing
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)         # -std=c++20, not gnu++20
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd / clang-tidy

option(MAIOLICA_NATIVE "Tune for this machine's CPU (-mcpu/-march=native). Binaries may not run elsewhere" OFF)
option(MAIOLICA_LTO "Link time optimisation in optimised builds (~5% faster)" ON)
option(MAIOLICA_MICROBENCH "Build microbenchmarks (fetches Google Benchmark)" OFF)
set(MAIOLICA_SANITIZE "" CACHE STRING "Sanitisers, e.g. 'address,undefined' or 'thread'")
set(MAIOLICA_PGO "" CACHE STRING "'generate', or the path to a .profdata file to optimise with")

find_package(Threads REQUIRED)

# Everything but main.cpp, so tests and benchmarks can link the engine too
add_library(maiolica_core OBJECT
        src/defs.h src/data.cpp src/init.cpp
        src/io.cpp src/uci.cpp
        src/board.cpp src/bitboards.cpp src/hashkeys.cpp
        src/attack.cpp src/movegen.cpp src/validate.cpp src/makemove.cpp src/misc.cpp
        src/search.cpp src/pvtable.cpp src/evaluate.cpp
        src/perft.cpp src/bench.cpp src/console.cpp
        )
target_include_directories(maiolica_core PUBLIC src)
target_compile_features(maiolica_core PUBLIC cxx_std_20)
target_compile_definitions(maiolica_core PUBLIC $<$<CONFIG:Debug>:DEBUG>) # turns on ASSERT()
target_compile_options(maiolica_core PUBLIC -Wall -Wextra -Wshadow -Wimplicit-fallthrough -Wno-unused-parameter)
target_link_libraries(maiolica_core PUBLIC Threads::Threads)

if(MAIOLICA_NATIVE) # on an M1 this is already the default CPU, so it changes nothing there
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        target_compile_options(maiolica_core PUBLIC -mcpu=native)
    else()
        target_compile_options(maiolica_core PUBLIC -march=native)
    endif()
endif()

if(MAIOLICA_SANITIZE)
    target_compile_options(maiolica_core PUBLIC -fsanitize=${MAIOLICA_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(maiolica_core PUBLIC -fsanitize=${MAIOLICA_SANITIZE})
endif()

if(MAIOLICA_PGO STREQUAL "generate")
    target_compile_options(maiolica_core PUBLIC -fprofile-instr-generate)
    target_link_options(maiolica_core PUBLIC -fprofile-instr-generate)
elseif(MAIOLICA_PGO)
    target_compile_options(maiolica_core PUBLIC -fprofile-instr-use=${MAIOLICA_PGO} -Wno-profile-instr-out-of-date)
endif()

add_executable(maiolica src/main.cpp)
target_link_libraries(maiolica PRIVATE maiolica_core)
if(MINGW) # self-contained windows .exe
    target_link_options(maiolica PRIVATE -static)
endif()

# -- Tests: perft tiers (smoke is small enough for debug/sanitiser builds) and the bench signature --
enable_testing()
add_executable(perft_tests tests/perft_tests.cpp)
target_link_libraries(perft_tests PRIVATE maiolica_core)
foreach(tier smoke quick full)
    add_test(NAME perft_${tier} COMMAND perft_tests ${tier})
    set_tests_properties(perft_${tier} PROPERTIES LABELS ${tier})
endforeach()

add_executable(fen_tests tests/fen_tests.cpp)
target_link_libraries(fen_tests PRIVATE maiolica_core)
add_test(NAME fen COMMAND fen_tests)
set_tests_properties(fen PROPERTIES LABELS "smoke;quick")

add_test(NAME uci COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/uci_test.sh $<TARGET_FILE:maiolica>)
set_tests_properties(uci PROPERTIES LABELS quick)

# console & replay: san moves (castles, promotions, en passant, disambiguation) & a quick game.
# These grep for the line they want under pipefail, so a crash after printing it still fails the test
add_test(NAME replay COMMAND bash -o pipefail -c "'$<TARGET_FILE:maiolica>' replay '${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_test.pgn' 0 | grep -E \"move 4: Kd7|couldn't read\"")
set_tests_properties(replay PROPERTIES LABELS quick FAIL_REGULAR_EXPRESSION "couldn't read")
add_test(NAME console COMMAND bash -o pipefail -c "printf 'time 0.2\\ne4\\nquit\\n' | '$<TARGET_FILE:maiolica>' console | grep -E 'Black plays [a-h][1-8][a-h][1-8]'")
add_test(NAME console_typo COMMAND bash -o pipefail -c "printf 'force\\nnf3\\nquit\\n' | '$<TARGET_FILE:maiolica>' console | grep 'not a legal move or command: nf3'")
set_tests_properties(console console_typo PROPERTIES LABELS quick)

file(STRINGS tests/bench_signature.txt MAIOLICA_BENCH LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS tests/bench_signature.txt) # so an edit is picked up by the next build
add_test(NAME bench_signature COMMAND bash -o pipefail -c "'$<TARGET_FILE:maiolica>' bench | grep -x 'Nodes searched  : ${MAIOLICA_BENCH}'")
set_tests_properties(bench_signature PROPERTIES LABELS quick) # too slow for debug builds

# -- Optional microbenchmarks, for iterating on hot functions (bench/perft still decide) --
if(MAIOLICA_MICROBENCH)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.1
            GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(benchmark)
    add_executable(microbench tests/microbench.cpp)
    target_link_libraries(microbench PRIVATE maiolica_core benchmark::benchmark)
endif()

# LTO last, so it applies to everything above
if(MAIOLICA_LTO AND NOT MAIOLICA_SANITIZE)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg)
    if(ipo_ok)
        foreach(target maiolica_core maiolica perft_tests fen_tests)
            set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
            set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        endforeach()
    else()
        message(STATUS "LTO not supported: ${ipo_msg}")
    endif()
endif()
