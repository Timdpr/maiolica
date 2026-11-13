# Testing & measuring

Each kind of claim has its own tool. Use the one that matches what a change claims.

| Claim | Proof | Tool |
|---|---|---|
| "Movegen is still correct" | perft counts match published values | `ctest` (perft tiers) |
| "Search is unchanged" | bench node count identical | `maiolica bench`, `tests/bench_signature.txt` |
| "Search is unchanged, broadly" | every search line identical over thousands of positions | `scripts/difftest.py` |
| "It's faster" | fewer instructions/cycles over interleaved runs | `scripts/ab.sh` |
| "It's stronger" (or no weaker) | SPRT passes | `scripts/match.sh` |
| "It's N Elo stronger than the baseline" | a fixed-length match | `scripts/match.sh <baseline-tag> elo` |
| "The protocol still works" | scripted sessions behave | `ctest` (`uci`, `fen`) |

`ctest --preset release` runs perft smoke and quick, `fen`, `uci`, `replay`, `console`, `console_typo` (a mistyped move is refused, not guessed at) and the bench signature (~15s). `replay` plays `tests/replay_test.pgn` (both castles, promotions, underpromotion, disambiguation, en passant, annotation codes like `$1`, a FEN start) through the san reader. The debug and asan presets run perft smoke and `fen`.

## Protocol
- **`tests/uci_test.sh <engine>`** pipes scripted sessions into the engine. A `~N` line sleeps N seconds, so commands can arrive mid-search. Each session has a 20s watchdog, and a crash, hang or non-zero exit fails the test.
  - It covers the handshake, the normal game flow, stop/infinite/ponder ordering, mate and null bestmoves, clock edge cases, messy input and the dev commands.
  - Run it against the `tsan` build to check the threading: `TSAN_OPTIONS="halt_on_error=1 exitcode=66" tests/uci_test.sh cmake-build-tsan/maiolica`.
- **`tests/fen_tests.cpp`** checks that every test and bench FEN parses, that malformed FENs are refused without touching memory out of bounds, and that clocks, castling rights and en passant squares are cleaned up.
- **Real games:** every `scripts/match.sh` run counts the PGN `Termination` tags when it finishes, and fails if any game ended by time forfeit, illegal move, crash or disconnect.

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

## Differential test
```sh
scripts/difftest.py master .       # master vs the working tree: 60 games, ~8,700 searches, ~6 min
scripts/difftest.py master . 10    # fewer games, for a quick look
```
The bench signature is one number from 9 positions, each on an empty table. The differential test is the wider version of the same check: it runs the same searches through two builds over UCI and compares every `info` and `bestmove` line, leaving out only `time` and `nps`. Both arguments are git refs, or `.` for the working tree.

It first makes its games: a few random opening moves, then the first build plays both sides at depth 4, with the odd random move thrown in. The random seed is fixed, so every run uses the same games. Each position is then searched by both builds, three ways:

| Mode | What it adds over bench |
|---|---|
| Fresh table, depth 6, every 3rd position | many more positions, with game history behind them (repetitions, the fifty-move count) |
| 1MB table kept for the whole game, 30000 nodes a move | a table that is always full, so entries get replaced all the time; node limits |
| 64MB table kept for the whole game, depth 7 (first 20 games) | the default table carried from move to move, a bit deeper |

It prints the first few mismatches per mode and exits non-zero if there were any. It only makes sense for a change that claims to leave search alone: once the bench signature moves, this fails too, as it should.

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

## Strength
```sh
scripts/match.sh master gain     # SPRT, elo0=0 elo1=10: does it gain?
scripts/match.sh master nonreg   # SPRT, elo0=-10 elo1=0: doesn't lose (simplifications, bug fixes)
scripts/match.sh <ref> elo       # 2000 games, no SPRT: how big is the gap?
```
The script builds the working tree ("dev") and the given ref ("base"). It plays 8+0.08 games (8 seconds each, plus 0.08 a move) with Hash 16, from `books/8moves_v3.pgn`, in pairs with colours swapped, 6 at a time. Results land in `matches/<date>-<ref>/`.

- **`gain` and `nonreg`** are SPRTs (sequential probability ratio tests): the match goes on only until the result is statistically clear either way, then stops. That answers "is it better?" in the fewest games. It uses the logistic model with α = β = 0.05, so it gives the wrong answer about 1 time in 20.
- **`elo`** plays a fixed number of games and reports the Elo difference with its error margin. Use it to measure a gap. Don't read the gap off an SPRT: because an SPRT stops the moment the score looks convincing, the Elo it shows at that point is biased. `GAMES=500 scripts/match.sh <ref> elo` changes the length. 2000 games take about 2 hours and give a margin of roughly ±9 Elo.
- **Seed:** the openings are drawn from the book at random. The seed is printed and saved in `info.txt`, and `SEED=<n> scripts/match.sh ...` replays a match with the same openings in the same order.
- **Terminations:** when the match finishes, the script counts how the games ended. Anything other than `normal` (a time forfeit, an illegal move, a crash, a disconnect) makes it exit non-zero, because that result wasn't decided on the board.
- **No adjudication:** games are played out to mate or a draw by the rules. Ending them early on the engines' own scores (resigning at -6.00, calling a draw when both sides sit near 0.00) was simulated on 790 real games: it would have got 17 to 20 results wrong, for 5 to 12% fewer moves played.

Setup is a one-off:
- Build fastchess in `external/fastchess`: `git clone --depth 1 https://github.com/Disservin/fastchess external/fastchess && make -C external/fastchess -j`.
- Unzip the book from official-stockfish/books into `books/`.

## Baseline
The baseline is a fixed version of the engine that later versions are measured against. It is the git tag **`baseline-2026-11`** (bench signature `4527030`).

The conditions never change, so that results stay comparable: 8+0.08, Hash 16, `books/8moves_v3.pgn`, 6 games at once, no adjudication. These are the script's defaults, so don't override them for a baseline run.

The routine:
1. Every change to the engine gets an SPRT against the previous master: `scripts/match.sh master gain` (or `nonreg`).
2. After each larger batch of work, measure the total gain with `scripts/match.sh baseline-2026-11 elo` and add a row here.

| Date | Version | Bench | Games | Elo vs baseline |
|---|---|---|---|---|
| 2026-11-13 | `baseline-2026-11` | 4527030 | | 0 (by definition) |

## Sanitisers
- `ctest --preset asan` runs Debug with ASan and UBSan.
- The `tsan` preset is for the search thread: drive it over UCI.
- For a full search check, run `cmake-build-asan/maiolica bench` (slow, ~3 min). It must print the same signature as release. For the same reason, `tests/uci_test.sh` against the asan build fails its `bench` session on the 20s watchdog; that one is expected.
