#!/usr/bin/env bash
# Profile guided release build (~3-5% faster): instrument -> run bench & perft -> merge -> rebuild.
# Result: cmake-build-pgo/maiolica. Clang only; needs llvm-profdata (on macOS, 'xcrun llvm-profdata').
set -euo pipefail
cd "$(dirname "$0")/.."
PROFDATA=${PROFDATA:-$(xcrun --find llvm-profdata 2>/dev/null || command -v llvm-profdata)}
gen=cmake-build-pgo-gen out=cmake-build-pgo

# build an instrumented engine, and run it on some typical work
rm -rf "$gen/profiles" && mkdir -p "$gen/profiles"
cmake -S . -B "$gen" -DCMAKE_BUILD_TYPE=Release -DMAIOLICA_PGO=generate >/dev/null
cmake --build "$gen" -j >/dev/null
LLVM_PROFILE_FILE="$PWD/$gen/profiles/%p.profraw" "$gen/maiolica" bench >/dev/null
LLVM_PROFILE_FILE="$PWD/$gen/profiles/%p.profraw" "$gen/perft_tests" quick >/dev/null
"$PROFDATA" merge -o "$PWD/$gen/maiolica.profdata" "$gen"/profiles/*.profraw

# then rebuild using the profile
cmake -S . -B "$out" -DCMAKE_BUILD_TYPE=Release -DMAIOLICA_PGO="$PWD/$gen/maiolica.profdata" >/dev/null
cmake --build "$out" -j >/dev/null
"$out/maiolica" bench | tail -3
