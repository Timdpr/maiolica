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
| "Several threads still work" | forced results at 1 to 8 threads, and no report from the thread sanitiser | `ctest` (`threads`), `ctest --preset tsan` |
| "N threads are stronger than one" | a match with dev on N threads | `THREADS=N scripts/match.sh` |

`ctest --preset release` runs perft smoke and quick, `fen`, `search`, `threads`, `eval`, `uci`, `replay`, `console`, `console_typo` (a mistyped move is refused, not guessed at), `console_time` (a move given a set time uses it) and the bench signature (~15s). `replay` plays `tests/replay_test.pgn` (both castles, promotions, underpromotion, disambiguation, en passant, annotation codes like `$1`, a FEN start) through the san reader. The debug, asan and tsan presets run perft smoke, `fen`, `search`, `threads` and `eval`.

`search` (`tests/search_tests.cpp`) searches five positions a few plies deep, about 200,000 nodes. It is there for the debug and asan presets: perft never makes a null move, and bench is too slow for those builds. In a debug build every `makeMove` in it checks the short legality test against the full one, `isInCheck` checks its remembered answer against a fresh one, and `checkBoard` checks the piece lists. A failed `ASSERT` aborts, which fails the test. In any build it fails if the search made no null-move cutoffs, so it can't quietly stop covering them. It also plants a table entry whose move isn't legal in the position, and checks that `probePonderMove` won't offer it as the ponder move. Two more checks are for the table: an entry is stored and read back with the widest value of every field (both mate scores, the deepest depth, each kind of score, a castling move and a capturing promotion, and an age on either side of where it wraps); and a deeper upper-bound entry is planted for the position to be searched, carrying a move that throws a mate away, to see that the search plays its own move and not that one.

## Several threads
With one search thread, the same search always visits the same nodes, which is what the bench signature and the differential test rely on. With more than one it doesn't: which thread writes a table entry first is down to timing, so node counts differ from run to run and now and then so does the move. Neither tool can say anything about such a search. What they can do is prove that **one thread is untouched**: `Threads` defaults to 1, `bench` always uses 1, and a change to the threading or the table must leave the signature and `scripts/difftest.py` where they were.

For more than one thread, the tests check things that must be true however the threads fall:
- **`threads`** (`tests/thread_tests.cpp`) searches five positions to a fixed depth twice over at 1, 2, 4 and 8 threads. A mate in one and a mate in two must be found with the mating move and the right score; every move played must be legal and be the first move of the last line printed; the node total must be at least the main thread's own count (and equal to it at one thread). Then a node limit at four threads must end the search, and so must a stop from another thread during `go infinite`. Nothing in it is timed, so it can't fail on a slow machine; a search that never comes back fails on the test's time limit. In a debug build every `ASSERT` runs in every thread.
- **`uci`** has twelve sessions at `Threads 4`: the option itself, one `info` line a depth with `bestmove` the last line's first move, a mate score, `stop` in mid search and straight after `go`, `go infinite` and `go ponder` waiting to be told, `quit` in mid search, a node limit, a new `Hash` after a threaded search, the usual node count once `Threads` is back at 1 and the table cleared, and `bench` giving the signature whatever `Threads` says.
- **`ctest --preset tsan`** runs the smoke tests in a build with the thread sanitiser, which watches every memory access and reports any place where two threads touch the same memory without it being atomic. One report fails the test (`halt_on_error`). CI runs it on Linux. The whole uci test can be run against that build too: see "Protocol" below.
- **Speed with threads** is time to depth: how long the same positions take to reach the same depth. `scripts/ab.sh` can't measure it (it runs `bench`, which is one thread).
- **Strength with threads** is a match: `THREADS=2 scripts/match.sh master gain`. See "Strength" below.

What the threads measured when they went in (Nov 2026, M1 Pro, 8+0.08, Hash 16; the version with threads, `b8293f2`, against the one before it, `3e588e9`, which always has one):

| Threads | Games | Wins-losses-draws | Elo | Kind of match | Time to depth 14 |
|---|---|---|---|---|---|
| 1 | 460 | 152-95-213 | +43.3 ± 24.5 | SPRT, passed "not weaker" | 1 (by definition) |
| 2 | 270 | 104-50-116 | +70.4 ± 27.7 | SPRT, passed "stronger" | 1.52 times as fast |
| 4 | 300 | 143-27-130 | +141.7 ± 27.6 | fixed length | 2.18 times as fast |
| 8 | not played | | | | 2.89 times as fast |

- The one-thread row is not the threads: the same change made the table entry 16 bytes, so the table holds twice as many. That gain is inside the other rows too.
- The first two rows are SPRT figures, which read high (see "Strength").
- Time to depth is over 150 positions from self-play games, against one thread of the same build.

`eval` (`tests/eval_tests.cpp`) checks that a position scores the same as its colour-flipped twin (22 positions: pawns for both sides, one side and neither), that the pawnless-draw shortcut fires only with no pawns on the board, and that the king prefers the middle in an ending and home in a middlegame. The bench signature can't do this job: a change that breaks one of these just gives a new number.

## Protocol
- **`tests/uci_test.sh <engine>`** pipes scripted sessions into the engine. A `~N` line sleeps N seconds, so commands can arrive mid-search. Each session has a 20s watchdog, and a crash, hang or non-zero exit fails the test.
  - It covers the handshake, the normal game flow, stop/infinite/ponder ordering, mate and null bestmoves, a pv that ends at a repetition, clock edge cases, messy input and the dev commands.
  - Run it against the `tsan` build to check the threading, both the input thread against the search and the search threads against each other: `TSAN_OPTIONS="halt_on_error=1 exitcode=66" tests/uci_test.sh cmake-build-tsan/maiolica`.
- **`tests/fen_tests.cpp`** checks that every test and bench FEN parses, that malformed FENs are refused without touching memory out of bounds, and that clocks, castling rights and en passant squares are cleaned up.
- **Real games:** every `scripts/match.sh` run counts the PGN `Termination` tags when it finishes, and fails if any game ended by time forfeit, illegal move, crash or disconnect.

## Perft
`tests/perft_tests.cpp` covers the six CPW positions (plus mirrored pos4) and Peter Ellis Jones' tricky positions (en passant pins, castling through check, promotions, stalemate). Counts come from [CPW Perft Results](https://www.chessprogramming.org/Perft_Results). Nine more positions are the engine's own, aimed at `makeMove`'s legality test: a pinned piece on each of the king's eight lines (with either side to move), en passant along, off and across a pin, a promotion by a pinned pawn, two squares that are far apart but not in line, and pins from seven squares away along every kind of line. Their counts were checked against a second, separately written move generator. Never add a count that hasn't been published or cross-checked with another engine.

| Tier | Where it runs | Time (M1 Pro) |
|---|---|---|
| `smoke` | debug and asan presets, where `ASSERT(checkBoard)` runs at every node | 0.2s release, 25s asan |
| `quick` | release preset and CI | ~1.7s |
| `full` | `ctest --preset full` | ~20s |

## Bench signature
`./maiolica bench` searches 9 fixed positions to fixed depths, each on a fresh board with a cleared 16MB table, and prints the total node count.

It is deterministic because the Zobrist keys come from a fixed-seed splitmix64. It's identical in Release, Debug and ASan builds, and should be identical across platforms; CI checks Linux (gcc), macOS (Apple clang) and Windows (MinGW gcc).

- **Refactor or speedup:** the count must not change. If it does, the change altered search: find out why before going on.
- **Intentional search or eval change:** update `tests/bench_signature.txt` and add `Bench: N` to the commit message.
- Positions without history can't show repetition or fifty-move fixes. Test those through UCI.

## Differential test
```sh
scripts/difftest.py master .       # master vs the working tree: 60 games, ~8,500 searches, ~2.5 min
scripts/difftest.py master . 10    # fewer games, for a quick look
```
The bench signature is one number from 9 positions, each on an empty table. The differential test is the wider version of the same check: it runs the same searches through two builds over UCI and compares every `info` and `bestmove` line, leaving out only `time` and `nps`. Both arguments are git refs, or `.` for the working tree.

It first makes its games: a few random opening moves, then the first build plays both sides at depth 4, with the odd random move thrown in. Every fifth game starts from a quiet endgame instead, with the fifty-move count already at 90 to 99, because a game from the start position never gets near that rule. The random seed is fixed, so every run uses the same games. Each position is then searched by both builds at the same time, three ways:

| Mode | What it adds over bench |
|---|---|
| Fresh table, depth 6, every 3rd position | many more positions, with game history behind them (repetitions, the fifty-move count) |
| 1MB table kept for the whole game, 30000 nodes a move | a table that is always full, so entries get replaced all the time; node limits |
| 64MB table kept for the whole game, depth 7 (first 20 games) | the default table carried from move to move, a bit deeper |

It prints the first few mismatches per mode and exits non-zero if there were any. An engine that crashes, or gives no answer for 2 minutes, fails it too. It only makes sense for a change that claims to leave search alone: once the bench signature moves, this fails too, as it should.

CI runs it on every pull request, on Linux only: 10 games (~1,600 searches) through the pull request and through the commit it would merge into, in under 2 minutes alongside the other tests. A pull request that changes `tests/bench_signature.txt` has changed search on purpose, so there the job compares nothing and passes.

## Speed
```sh
scripts/ab.sh HEAD . 9          # last commit vs working tree, 9 interleaved runs of bench
scripts/ab.sh master HEAD 9 perft
```
- **Instructions retired:** nearly noise-free, the metric for identical-search changes. A self-test of the same code shows ±0.00%.
- **Cycles and wall time:** wobble by ~1–3% when the laptop is busy.
- **Microbenchmarks:** `cmake --preset microbench && cmake --build --preset microbench && cmake-build-microbench/microbench` times movegen, make/undo, attacks and eval in isolation. They're good for iterating, but LTO inlines these functions into the real search, so only `ab.sh` decides.
- **Profiling:** build the `profile` preset, then `sample` or Instruments on `maiolica bench`.

### The fastest build (PGO)
```sh
scripts/pgo.sh      # ~10s; the engine is cmake-build-pgo/maiolica
```
Profile-guided optimisation (PGO) is a build in two passes. The first makes an engine that counts how often each branch and call is taken, and runs it on `bench` and the quick perft. The second compiles again with those counts, so the compiler lays out the code for the paths that are really used. **This is the build to play or analyse with:** point a GUI at `cmake-build-pgo/maiolica`.

- **Gain** over the release preset (M1 Pro, Apple clang 17, minimum of 7 interleaved runs): bench −3.8% instructions and −3.5% cycles; perft −5.9% instructions and −10.8% cycles.
- **Same search:** only the speed may change. The script ends by running `bench` and fails if the node count isn't the one in `tests/bench_signature.txt`.
- **Run it again after changing the code.** The counts belong to the source they were collected from; a stale profile still builds, but fits the code less well.
- **Clang only**, and it needs `llvm-profdata` (`xcrun llvm-profdata` on macOS; the script finds it, or set `PROFDATA`).
- **Not for comparing versions.** `ctest`, `ab.sh`, `difftest.py` and `match.sh` all build the plain release preset, so two versions are always compared like for like. A speedup measured with `ab.sh` is a speedup of the release build; the PGO build usually keeps most of it, but measure if it matters.

## Strength
```sh
scripts/match.sh master gain     # SPRT, elo0=0 elo1=10: does it gain?
scripts/match.sh master nonreg   # SPRT, elo0=-10 elo1=0: doesn't lose (simplifications, bug fixes)
scripts/match.sh <ref> elo       # 2000 games, no SPRT: how big is the gap?
CAP=2000 scripts/match.sh master nonreg   # the same SPRT, but stop at 2000 games if it still hasn't decided
NODES=30000 scripts/match.sh master nonreg   # a quick first look, with no clock: every move searches 30,000 nodes
THREADS=2 CAP=2000 scripts/match.sh master gain   # dev on two search threads against base on one
```
The script builds the working tree ("dev") and the given ref ("base"). It plays 8+0.08 games (8 seconds each, plus 0.08 a move) with Hash 16, from `books/8moves_v3.pgn`, in pairs with colours swapped, 6 at a time.

- **`gain` and `nonreg`** are SPRTs (sequential probability ratio tests): the match goes on only until the result is statistically clear either way, then stops. That answers "is it better?" in the fewest games. It uses the logistic model with α = β = 0.05, so it gives the wrong answer about 1 time in 20.
- **`elo`** plays a fixed number of games and reports the Elo difference with its error margin. Use it to measure a gap. Don't read the gap off an SPRT: because an SPRT stops the moment the score looks convincing, the Elo it shows at that point is biased. `GAMES=500 scripts/match.sh <ref> elo` changes the length. 2000 games take about 2 hours and give a margin of roughly ±9 Elo between two versions of about the same strength. The margin widens as the gap does: it was ±19 at +387.
- **`CAP=<games>`** puts an upper limit on a `gain` or `nonreg` run. Decide on one before starting: see "How long an SPRT takes" below. Without one the script says so in `info.txt`.
- **`NODES=<n>`** takes the clock away and gives every move the same number of nodes instead. It is a quick first look, not a verdict: see "A quick look at fixed nodes" below.
- **`THREADS=<n>`** sets dev's `Threads` option; base keeps its one thread. It is for testing the threads themselves, so the base should be a version with the same search. Only one side thinks at a time, so a game keeps at most n threads busy, and the script plays fewer games at once to keep that under six in all: 3 games for two threads (about 8 games a minute at 8+0.08, so 2,000 take about 4 hours), 1 for four (under 3 a minute). `CONCURRENCY` overrides it. More busy threads than fast cores hurts the side with more threads most, so a crowded machine makes the gain read low. The thread count goes into `info.txt` and the results table. It can't be combined with `NODES`. Without it neither engine is given the option, and both search with one thread.
- **Seed:** the openings are drawn from the book at random. The seed is printed and saved in `info.txt`, and `SEED=<n> scripts/match.sh ...` replays a match with the same openings in the same order.
- **Terminations:** when the match finishes, the script counts how the games ended. Anything other than `normal` (a time forfeit, an illegal move, a crash, a disconnect) makes it exit non-zero, because that result wasn't decided on the board.
- **A stale base:** `master` here is your local copy of the branch, which only moves when you pull. The script fetches first, and if the base branch is behind its remote copy it says so on screen and in `info.txt`. Using `origin/master` as the base avoids the question.
- **On screen:** one line every 100 games, with the score so far, the Elo and a guess at the time left, then the final result. Every game is in the log.
- **A steady machine:** the laptop is kept awake for the length of the match. If it was on battery, in low power mode or already busy when the match started, a warning goes on screen and into `info.txt`, since any of those can skew a result.
- **Pausing:** Ctrl-C pauses a match. Every finished game is kept; the few in progress are dropped. The row in the table says `paused at N games`, and the script prints the command that carries on: `RESUME=<folder> scripts/match.sh`. That takes no other arguments, since the engines, openings, settings and score so far are read back from the folder. The dropped games are played again from their openings, and the match runs to the length it was started with, so a paused match is not a shortened one. Checked with a 200-game fixed-node match paused twice: it ended with the same games as the one played straight through. `games.pgn` can hold one game twice, if it finished at the moment of the pause; the score counts it once.
- **A cap:** a run that reaches its `CAP` says `capped at N games`, with the LLR it had got to.
- **No adjudication:** games are played out to mate or a draw by the rules. Ending them early on the engines' own scores (resigning at -6.00, calling a draw when both sides sit near 0.00) was simulated on 790 real games: it would have got 17 to 20 results wrong, for 5 to 12% fewer moves played.

### How long an SPRT takes, and when to cap it
An SPRT has no fixed length. It is quick when the change is clearly on one side of its two bounds, and slow when the truth lies between them. Expected lengths at our usual 16 games a minute, worked out from the spread of scores in our own games (estimates, not promises: a single run can take half or twice as long):

| What the change is really worth | `nonreg` [-10, 0] | `gain` [0, 10] |
|---|---|---|
| -10 Elo | fails after ~2,650 games (2.8h) | fails after ~900 (0.9h) |
| -5 | a coin flip after ~3,900 (4.1h) | fails after ~1,300 (1.4h) |
| 0 | passes after ~2,650 (2.8h) | fails after ~2,650 (2.8h) |
| +5 | passes after ~1,300 (1.4h) | a coin flip after ~3,900 (4.1h) |
| +10 | passes after ~900 (0.9h) | passes after ~2,650 (2.8h) |
| +20 | passes after ~500 (0.6h) | passes after ~900 (0.9h) |

So before starting one, ask what the run is for and what it is likely to cost, and choose a cap (or choose to go without):

- **`nonreg` decides whether a change stays in**, so by default it runs to its verdict. A cap is reasonable when there is other evidence that the change can't cost strength, such as a fix whose effect on the search has been checked directly. If the cap is reached, read the LLR in the verdict: above 0 means the score is nearer the pass bound than the fail bound.
- **A cap makes the test less sure.** At 2,000 games, judged by the sign of the LLR, a change that is really 10 Elo weaker gets through about 15% of the time, against 5% for a run to the verdict. A harmless change is wrongly flagged about as often. A shorter cap costs more.
- **`gain` after a `nonreg` pass decides nothing**: the change stays either way, and the run only upgrades "not weaker" to "stronger". It is cheap when `nonreg` passed in under about 700 games, which is what a change worth +15 or more does. Otherwise skip it, or cap it at 2,000 games and report the Elo with its margin, not a pass or fail.
- **A new feature has to pass `gain` to go in**, so there the run is the decision, and a cap is a way of saying "not worth more games than this".
- **Set the cap before the match starts.** Stopping a run because of the score it happens to show is how a test gets bent.
- **2,000 games is the natural cap**: by then the Elo figure is good to about ±9, and twice as many games only brings that to about ±7.
- **A baseline match is never capped or cut short.** It is `elo` mode, which has a fixed length already.

### A quick look at fixed nodes
`NODES=30000 scripts/match.sh master nonreg` plays the same kind of match with no clock: each move searches 30,000 nodes (positions) and then has to answer. A run takes a minute or two, where a timed SPRT takes hours.

- **What it is for:** finding out early that a change is broken or clearly worse, before a timed match is spent on it.
- **What it can't see:** anything about speed or time management, since both sides get the same nodes however long they take over them. It can't see the size of the table either, which never fills at this depth. A gain at 30,000 nodes (about depth 6) needn't be the same size in a real game (about depth 9). So a change still has to pass the timed test to go in.
- **The machine's load doesn't matter.** The same 200 games played on a quiet machine and on one with every core busy came out identical, move for move. The battery, low-power and busy warnings are left out of `info.txt` for these runs, and `CONCURRENCY` can go up to the number of cores.
- **Checked against three results already known from timed matches** (Nov 2026, 30,000 nodes, 3,000 games each):

  | Change | Timed SPRTs at 8+0.08 | Fixed nodes |
  |---|---|---|
  | King's endgame table in use (`6cacedd`) | passed, +82 to +98 Elo | +71.0 ± 8.1 |
  | Pawnless-draw check fixed (`d23669b`) | passed, +14 to +25 | +7.6 ± 2.6 |
  | En passant square only when capturable (`0c91a9d`) | level, -0.5 ± 7.6 | +1.2 ± 5.8 |

  The order and the signs agree; the gains come out smaller. 10,000 nodes gave the same picture (+67, +5, +0.5).
- **It is still an SPRT, so it is wrong about 1 time in 20.** As `nonreg` runs, the two gains passed in 200 to 300 games, and failed when old and new were swapped. The level change passed three times and failed once. When a run fails a change you have reason to think is harmless, run it again before believing it: it costs a minute.
- **The margins look small because the games come in pairs.** With no clock, two engines that differ only slightly play the same game twice with colours swapped unless the change comes into it, and those pairs cancel out exactly.
- The engine checks the limit every 2,048 nodes, so a count below that behaves like 2,048.

Setup is a one-off:
- Build fastchess in `external/fastchess`, at the commit the results so far were played with (alpha 1.8.3): `git clone https://github.com/Disservin/fastchess external/fastchess && git -C external/fastchess checkout 2a2aa4e && make -C external/fastchess -j`.
- Unzip the book from official-stockfish/books into `books/`.

### Where results go
Every match gets a folder in `matches/`, named `<date>-<time>-<dev commit>-vs-<base ref>-<mode>` (the commit gets `+dirty` if there were uncommitted changes). `matches/`, fastchess and the book are always the main checkout's, so the script works the same from a git worktree and all results end up in one place.

| File | What it is |
|---|---|
| `info.txt` | what was compared, the conditions, the seed, the fastchess version, any warnings, and when it was paused and resumed |
| `result.txt` | fastchess's final score, Elo and SPRT verdict, and how the games ended |
| `games.pgn` | every game, with each move's score, depth and time |
| `config.json` | fastchess's full settings and the score so far, which is what a resumed match starts from |
| `log.txt.gz` | fastchess's running output, compressed |
| `dev`, `base` | the two binaries that played |

Each match also adds a row to the table in `matches/README.md` (the file is created if it's missing): date, description, both commits, mode, time control, games, wins-losses-draws, Elo, verdict and folder. `WHAT="null move tweak" scripts/match.sh master gain` fills in the description. The folder is gitignored, so these are local records.

## Baseline
A baseline is a fixed version of the engine that later versions are measured against. It is a git tag. There have been two, and **new matches are played against the newer one**:

| Tag | Commit | Bench | Where it stands |
|---|---|---|---|
| `baseline-2026-11` | `b834c5a` | 4527030 | the first: 0 by definition |
| **`baseline-2026-11-18`** | `c2b6c1c` | 440084 | +387.1 ± 19.4 against the first |

The second tag was made when the engine was taking 90% of the points against the first (52 losses in 2000 games). That far from level, a small change in the score is a large change in Elo, so the same 2000 games give a wider margin each time: ±9 when the two sides are level, ±19 at +387. Against a baseline of about its own strength the margin is back to ±9.

To place a later version on the first baseline's scale, add its figure to +387. That sum is rough: Elo gaps measured against different opponents don't add up exactly, and both margins come along.

The conditions never change, so that results stay comparable: 8+0.08, Hash 16, `books/8moves_v3.pgn`, 6 games at once, no adjudication. These are the script's defaults, so don't override them for a baseline run.

The routine:
1. Every change to the engine gets an SPRT against the previous master: `scripts/match.sh master gain` (or `nonreg`).
2. After each larger batch of work, measure the total gain with `scripts/match.sh baseline-2026-11-18 elo` and add a row here.

Against `baseline-2026-11-18`:

| Date | Version | Bench | Games | Elo vs baseline |
|---|---|---|---|---|
| 2026-11-18 | `baseline-2026-11-18` | 440084 | | 0 (by definition) |
| 2026-11-19 | `a842578` (a soft time limit, a 16-byte table entry and so twice the entries, late move pruning, quiescence never starting from a position in check; one thread) | 265768 | 2000 | +127.2 ± 12.2 (978 wins, 277 losses, 745 draws) |

The SPRTs that let those four changes in showed about +109, +43, +69 and +8, which add up to far more than +127. That is what "don't read the gap off an SPRT" above means in practice: a test that stops when the score looks convincing reports a figure on the high side, and the fixed-length match is the one to believe.

Against `baseline-2026-11` (closed: the last row is the version the second tag marks):

| Date | Version | Bench | Games | Elo vs baseline |
|---|---|---|---|---|
| 2026-11-13 | `baseline-2026-11` | 4527030 | | 0 (by definition) |
| 2026-11-14 | `969b6f5` (piece-list index, cheap legality test) | 4527030 | 2000 | +15.7 ± 10.3 (631 wins, 541 losses, 828 draws) |
| 2026-11-16 | 1.1, `8051995` (two eval fixes, an en passant square only when a pawn can take, a pv walk that leaves the search's board alone) | 3938564 | 2000 | +126.4 ± 12.7 (1108 wins, 411 losses, 481 draws) |
| 2026-11-17 | `c3307c9` (late move reductions) | 658098 | 2000 | +201.0 ± 14.2 (1289 wins, 246 losses, 465 draws) |
| 2026-11-18 | `b2ced09` (quiet moves scored by the cutoffs they caused, a formula for late move reductions, reverse futility and futility pruning, aspiration windows) | 440084 | 2000 | +387.1 ± 19.4 (1663 wins, 52 losses, 285 draws) |

The first baseline played itself under these conditions, to check the setup favours neither side: 2000 games came out at -2.3 ± 9.1 Elo (591 wins, 604 losses, 805 draws), every game ending normally. So a gap under about 9 Elo at this length is noise.

## Against other engines
Everything above measures the engine against an older copy of itself, which says how much it has gained and overstates what that gain is worth against anyone else. `scripts/outside.sh` gives a rough outside figure: it plays the working tree against other engines whose ratings are published.

```sh
scripts/outside.sh                      # 300 games against each engine in external/engines/list.txt (~2 hours for six)
GAMES=20 scripts/outside.sh             # a short trial, e.g. to see whether a new opponent loses on time
RESUME=<folder> scripts/outside.sh      # carry on after a Ctrl-C
```

- **Conditions** are the baseline's: 8+0.08, Hash 16, `books/8moves_v3.pgn`, 6 games at once, no adjudication, one thread, no opening books or tablebases of the engines' own.
- **One match per opponent**, each from the same openings. Ctrl-C pauses and `RESUME=` carries on, as for `match.sh`.
- **The figure** is a performance rating: the rating at which our total score against these opponents, at their published ratings, would be exactly the expected one. `result.txt` also shows each opponent separately, and how many games weren't decided on the board and who lost them.
- **The ratings** are from the [CCRL Blitz list](https://computerchess.org.uk/ccrl/404/), the best-known public list of engine ratings. CCRL plays at 2 minutes plus 1 second a move, on its own hardware, with 128 or 256MB tables and a general opening book; we play much faster, on a different machine, with a small table. Engines gain different amounts from extra time, so read the result as "roughly X on that scale, give or take 40", not as a rating. Only CCRL testing a release gives the real number.

### The opponents
C and C++ engines that build with the system compiler and speak UCI. They are a ladder, not a pool chosen around today's strength, so the same engines stay useful as the engine improves. Sources go in `external/engines/src/`, binaries in `external/engines/bin/`, and `external/engines/list.txt` has a line for each: name, rating, binary. All of that is gitignored.

| Engine | CCRL Blitz, 14 Nov 2026 | Source, at the tag CCRL rated | Built with |
|---|---|---|---|
| Stash 12.0 | 1882 | `github.com/mhouppin/stash-bot`, `v12` | `make CC=cc EXT_OFLAGS="-Wno-error -w"` |
| BBC 1.2 | 1945 | `github.com/maksimKorzh/bbc`, `1.2` | `cc -Ofast` on `src/bbc_1.2.c`, through a wrapper file that includes the system headers and then has `#define time bbc_time` before including the source: macOS declares `time()` where the engine has a global of that name |
| Stash 14.0 | 2058 | the same, `v14` | as Stash 12.0, in `src/` |
| CeeChess 1.3.2 | 2171 | `github.com/bctboi23/CeeChess`, `v1.3.2` | `cc -O3` on the `.c` files its makefile lists |
| Supernova 2.1.4 | 2259 | `github.com/MichaeltheCoder7/Supernova`, `v2.1.4` | `cc -std=c99 -O3 src/*.c -lpthread` in `Linux/` |
| Cinnamon 2.4 | 2326 | `github.com/gekomad/Cinnamon`, `v2.4` | `make cinnamon64-ARM COMP=clang++ LIBS=-lpthread` in `src/` |

Higher rungs for later, from the same list: Stash 17.0 (2296), CeeChess 1.4 (2362), Halogen 6 (2410), Stash 19.0 (2472).

### Results
| Date | Version | Games | Score | Performance |
|---|---|---|---|---|
| 2026-11-16 | 1.1 (`7bf997a`, bench 3938564) | 1800 | 39.5% (551 wins, 929 losses, 320 draws) | about 2020 (± 16 from luck alone) |

Opponent by opponent: 64.8% against Stash 12.0, 58.0% against BBC 1.2, 48.2% against Stash 14.0, 30.8% against CeeChess 1.3.2, 17.5% against Supernova 2.1.4 and 17.7% against Cinnamon 2.4. Taken one at a time they put the engine between 1988 and 2059. The engine lost no game on time and drew no warnings from fastchess. BBC lost 10 games on time and Stash 14.0 one; leaving those out moves the figure by about 2.

What the game records showed:
- **Raw speed is not the gap.** On one middlegame position for one second, the engine searched 8.9 million nodes a second, level with Stash 12.0 and 2.5 to 4 times the others. It reached depth 9 from 8.4 million nodes; Cinnamon reached depth 14 from 1.0 million. The stronger engines get 2 to 5 plies deeper per move by searching fewer, better-chosen positions.
- **A third of the losses turn on one move:** in 307 of 929, the engine's own score fell by 1.5 pawns or more in a single move from a position it thought was level or better. That is a tactic just past where it stopped looking.
- **About a fifth of the clock went unused** at this speed. The time for a move is a thirtieth of the clock, plus the increment, less `Move Overhead`, which was 50 ms when this match was played. So the clock settled near 30 × 50 ms and never dropped below it: the lowest it reached in any of 1,800 games was 1.34 seconds, and it usually ended with 2.3 of its 8 seconds left. At 0.17 seconds a move, 50 ms is a large share; at slow time controls it is nothing. The default has since come down to 20 ms: the delay fastchess actually measures on this machine is under 1 ms a move, though now and then far longer (195 ms at worst in 650,000 moves). With that and the two time limits (see [uci.md](uci.md)), the clock at this speed stays above about 0.6 seconds and ends a game with about 14% of its time unused. Most of the opponents keep a similar reserve.

## Sanitisers
- `ctest --preset asan` runs Debug with ASan and UBSan: perft smoke, `fen`, `search` and `eval`, with every `ASSERT` on (~1 min, half of it `threads`). CI runs it on Linux, on every pull request and every push to master.
- `ctest --preset tsan` runs RelWithDebInfo with the thread sanitiser: perft smoke, `fen`, `search`, `threads` and `eval` (~10s), failing on the first report. CI runs it on Linux. `threads` is the one that matters there; for the input thread against the search, drive the build over UCI (see "Protocol").
- For a full search check, run `cmake-build-asan/maiolica bench` (slow, ~3 min). It must print the same signature as release. For the same reason, `tests/uci_test.sh` against the asan build fails its `bench` session on the 20s watchdog; that one is expected.
