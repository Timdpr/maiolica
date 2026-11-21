# UCI

Maiolica speaks [UCI](https://www.shredderchess.com/download/div/uci.zip), so it plays in any UCI GUI (Cute Chess, En Croissant, Arena, Banksia, Nibbler…) or match runner (fastchess, cutechess-cli). At startup it prints a one-line banner, then everything it sends is protocol. `tests/uci_test.sh` checks the behaviour below.

## Commands

| Command | Notes |
|---|---|
| `uci` | `id`, the options below, `uciok` |
| `isready` | `readyok`, immediately, even mid search |
| `ucinewgame` | stops any search, clears the hash and the history scores kept from the last search, resets to the start position |
| `position startpos \| fen <fen> [moves …]` | Any whitespace. A bad FEN falls back to the start position. A bad or illegal move stops the move list there. Both report an `info string` |
| `go …` | see below. A running search is stopped first |
| `stop` | stops the search, which then sends `bestmove`. Harmless when idle |
| `ponderhit` | the move being pondered was played: the clock starts now |
| `setoption name <name> [value <v>]` | names aren't case sensitive |
| `debug on\|off` | `on` adds `info string`s (e.g. the time budget, which limit stopped the search, unknown commands) |
| `quit` | stops any search & exits. End of input does the same |

### `go`
`wtime btime winc binc movestogo movetime depth nodes infinite ponder`. `searchmoves` and `mate` are ignored.
- **Time budget:** `time / movestogo + inc - Move Overhead`, where movestogo is 30 if not given. It is capped at 3/4 of the clock left after overhead, and always at least 1 ms, which is also what a negative clock (an overrun) gets.
- **Two limits** come from the budget. The soft limit is 60% of it: once it has passed, no new depth is started, since a depth takes about twice as long as the one before and one that is cut short is thrown away. The hard limit is twice the budget, but no more than an eighth of the clock, and never less than the budget itself: the search is stopped dead there, checked every 2048 nodes. The eighth is there because whatever a move takes beyond its budget is more than the increment gives back, so a run of long moves would otherwise empty the clock. With it the clock can't be run down below where the budget alone would settle. Measured on game positions at 8+0.08 with plenty on the clock, a move typically takes 0.9 of its budget, and about one in nine runs to the hard limit. `movetime` sets both limits to `movetime - Move Overhead`.
- **`depth`** is clamped to 1–62. With no limits at all, it searches to depth 62.
- **`nodes`** stops within 2048 nodes of the limit (after `ponderhit`, if pondering). With more than one thread the limit is for all of them together and the search runs a few thousand nodes past it: the main thread checks the total every 2048 of its own nodes, and the helpers report theirs 2048 at a time.
- **`infinite`** and **`ponder`** never send `bestmove` until `stop` (or `ponderhit`, when both limits start running), even if the search finishes first.
- **`bestmove`** is always a legal move. If the search stops before depth 1 completes, it's the first legal move; with no legal moves it's `0000`. It adds `ponder <move>` when the hash table has a legal reply to it.

## Options

| Option | Type | Default | Range |
|---|---|---|---|
| `Hash` | spin (MB) | 64 | 1–2048 |
| `Clear Hash` | button | | |
| `Threads` | spin | 1 | 1–256 |
| `Ponder` | check | false | (tells the GUI we can ponder) |
| `Move Overhead` | spin (ms) | 20 | 0–5000 |

`Threads` is how many threads search. Each extra one searches the same position as the first and shares the hash table with it, so that what one works out the others find there (Lazy SMP). Set it to the number of fast cores you can spare, and give the table more room with it: four threads rewrite about a quarter of a 16MB table in a fifth of a second. With one thread the same position is always searched the same way; with more it isn't, as the threads' timing differs from run to run, so node counts and now and then the move chosen vary. The time limits, `stop` and `ponderhit` behave the same at any setting.

`Move Overhead` is taken off every move's budget, to cover the delay between the engine sending its move and the GUI or match runner stopping the clock. It also sets how low the clock can go: with an increment, the clock settles at about 30 times the part of the overhead that isn't needed. On one machine the delay is a few milliseconds and 20 is plenty. Playing over a network (a server, or a bot account), raise it to cover the slowest reply you expect.

## Output

`info depth D seldepth S score cp X|mate M nodes N nps P hashfull H time T pv …`, once per completed iteration.
- `nodes` and `nps` count every search thread; `depth`, `seldepth`, `score` and `pv` are the main thread's, and `bestmove` is always the first move of the last `pv`.
- `mate M` is in moves, negative when getting mated.
- `pv` ends with the move that repeats a position or reaches the fifty-move limit; the search scores that as a draw.
- `hashfull` is sampled from the first 1000 entries.
- Every line is written in one go under a mutex, so the input and search threads never interleave. Only the main search thread prints.

## Developer commands

Not part of UCI; handy from a terminal.

| Command | |
|---|---|
| `bench` | the fixed benchmark (same as `./maiolica bench`); ends with `N nodes N nps`. Always one thread, whatever `Threads` says |
| `perft N` / `go perft N` | perft from the current position, split by move, then `Nodes searched: N` |
| `d` | print the board |
| `eval` | static eval, in centipawns from the side to move |
| `run` | infinite analysis of the start position |
| `epd [file]` | analyse every position in an EPD file (default `lct2.epd`) |
