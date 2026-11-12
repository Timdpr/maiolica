# Testing & measuring

Three questions, three tools. Use the one that matches what a change claims.

| Claim | Proof | Tool |
|---|---|---|
| "Movegen is still correct" | perft counts match published values | `ctest` (perft tiers) |
| "Search is unchanged" | bench node count identical | `maiolica bench`, `tests/bench_signature.txt` |
| "It's faster" | fewer instructions/cycles over interleaved runs | `scripts/ab.sh` |
| "It's stronger" (or no weaker) | SPRT passes | `scripts/match.sh` |
| "The protocol still works" | scripted sessions behave | `ctest` (`uci`, `fen`) |

`ctest --preset release` runs perft smoke and quick, `fen`, `uci`, `replay`, `console`, `console_typo` (a mistyped move is refused, not guessed at) and the bench signature (~15s). `replay` plays `tests/replay_test.pgn` (both castles, promotions, underpromotion, disambiguation, en passant, a FEN start) through the san reader. The debug and asan presets run perft smoke and `fen`.

## Protocol
- **`tests/uci_test.sh <engine>`** pipes scripted sessions into the engine. A `~N` line sleeps N seconds, so commands can arrive mid-search. Each session has a 20s watchdog, and a crash, hang or non-zero exit fails the test.
  - It covers the handshake, the normal game flow, stop/infinite/ponder ordering, mate and null bestmoves, clock edge cases, messy input and the dev commands.
  - Run it against the `tsan` build to check the threading: `TSAN_OPTIONS="halt_on_error=1 exitcode=66" tests/uci_test.sh cmake-build-tsan/maiolica`.
- **`tests/fen_tests.cpp`** checks that every test and bench FEN parses, that malformed FENs are refused without touching memory out of bounds, and that clocks, castling rights and en passant squares are cleaned up.
- **Real games:** a few hundred fastchess self-play games, with `-recover` and a grep of the PGN `Termination` tags for time forfeits, illegal moves or disconnects.

## Perft
`tests/perft_tests.cpp` covers the six CPW positions (plus mirrored pos4) and Peter Ellis Jones' tricky positions (en passant pins, castling through check, promotions, stalemate). Counts come from [CPW Perft Results](https://www.chessprogramming.org/Perft_Results). Never add a count that hasn't been published or cross-checked with another engine.

| Tier | Where it runs | Time (M1 Pro) |
|---|---|---|
| `smoke` | debug and asan presets, where `ASSERT(checkBoard)` runs at every node | 0.2s release, 17s asan |
| `quick` | release preset and CI | ~1.7s |
| `full` | `ctest --preset full` | ~20s |

## Bench signature
`./maiolica bench` searches 9 fixed positions to fixed depths, each on a fresh board with a cleared 16MB table, and prints the total node count.

It is deterministic because the Zobrist keys come from a fixed-seed splitmix64. It's identical in Release, Debug and ASan builds, and should be identical across platforms; CI checks Linux and macOS.

- **Refactor or speedup:** the count must not change. If it does, the change altered search: find out why before going on.
- **Intentional search or eval change:** update `tests/bench_signature.txt` and add `Bench: N` to the commit message.
- Positions without history can't show repetition or fifty-move fixes. Test those through UCI.

## Speed
```sh
scripts/ab.sh HEAD . 9          # last commit vs working tree, 9 interleaved runs of bench
scripts/ab.sh master HEAD 9 perft
```
- **Instructions retired:** nearly noise-free, the metric for identical-search changes. A self-test of the same code shows ±0.00%.
- **Cycles and wall time:** wobble by ~1–3% when the laptop is busy.
- **Microbenchmarks:** `cmake --preset microbench && cmake --build --preset microbench && cmake-build-microbench/microbench` times movegen, make/undo, attacks and eval in isolation. They're good for iterating, but LTO inlines these functions into the real search, so only `ab.sh` decides.
- **Profiling:** build the `profile` preset, then `sample` or Instruments on `maiolica bench`.
- **PGO:** `scripts/pgo.sh` gives a profile-guided build in `cmake-build-pgo/` (~3–5% faster).

## Strength (SPRT)
```sh
scripts/match.sh master gain     # elo0=0 elo1=10: does it gain?
scripts/match.sh master nonreg   # elo0=-10 elo1=0: doesn't lose (simplifications, bug fixes)
```
The script builds the working tree and the base ref. It plays 8+0.08 games with Hash 16, from `books/8moves_v3.pgn`, in pairs with colours swapped, 6 at a time. The SPRT uses the logistic model with α = β = 0.05, and results land in `matches/<date>-<ref>/`.

Setup is a one-off:
- Build fastchess in `external/fastchess`: `git clone --depth 1 https://github.com/Disservin/fastchess external/fastchess && make -C external/fastchess -j`.
- Unzip the book from official-stockfish/books into `books/`.

## Sanitisers
- `ctest --preset asan` runs Debug with ASan and UBSan.
- The `tsan` preset is for the search thread: drive it over UCI.
- For a full search check, run `cmake-build-asan/maiolica bench` (slow, ~3 min). It must print the same signature as release.
