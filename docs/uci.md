# UCI

Maiolica speaks [UCI](https://www.shredderchess.com/download/div/uci.zip), so it plays in any UCI GUI (Cute Chess, En Croissant, Arena, Banksia, Nibbler…) or match runner (fastchess, cutechess-cli). At startup it prints a one-line banner, then everything it sends is protocol. `tests/uci_test.sh` checks the behaviour below.

## Commands

| Command | Notes |
|---|---|
| `uci` | `id`, the options below, `uciok` |
| `isready` | `readyok`, immediately, even mid search |
| `ucinewgame` | stops any search, clears the hash, resets to the start position |
| `position startpos \| fen <fen> [moves …]` | Any whitespace. A bad FEN falls back to the start position. A bad or illegal move stops the move list there. Both report an `info string` |
| `go …` | see below. A running search is stopped first |
| `stop` | stops the search, which then sends `bestmove`. Harmless when idle |
| `ponderhit` | the move being pondered was played: the clock starts now |
| `setoption name <name> [value <v>]` | names aren't case sensitive |
| `debug on\|off` | `on` adds `info string`s (e.g. the time budget, unknown commands) |
| `quit` | stops any search & exits. End of input does the same |

### `go`
`wtime btime winc binc movestogo movetime depth nodes infinite ponder`. `searchmoves` and `mate` are ignored.
- **Time budget:** `time / movestogo + inc - Move Overhead`, where movestogo is 30 if not given. It is capped at 3/4 of the clock left after overhead, and always at least 1 ms, which is also what a negative clock (an overrun) gets. `movetime` uses `movetime - Move Overhead`. The search stops on the budget, checked every 2048 nodes.
- **`depth`** is clamped to 1–62. With no limits at all, it searches to depth 62.
- **`nodes`** stops within 2048 nodes of the limit (after `ponderhit`, if pondering).
- **`infinite`** and **`ponder`** never send `bestmove` until `stop` (or `ponderhit`, then the budget runs out), even if the search finishes first.
- **`bestmove`** is always a legal move. If the search stops before depth 1 completes, it's the first legal move; with no legal moves it's `0000`. It adds `ponder <move>` when the PV has a second move.

## Options

| Option | Type | Default | Range |
|---|---|---|---|
| `Hash` | spin (MB) | 64 | 1–2048 |
| `Clear Hash` | button | | |
| `Threads` | spin | 1 | 1–1 (accepted for tools that always set it) |
| `Ponder` | check | false | (tells the GUI we can ponder) |
| `Move Overhead` | spin (ms) | 50 | 0–5000 |

## Output

`info depth D seldepth S score cp X|mate M nodes N nps P hashfull H time T pv …`, once per completed iteration.
- `mate M` is in moves, negative when getting mated.
- `hashfull` is sampled from the first 1000 entries.
- Every line is written in one go under a mutex, so the input and search threads never interleave.

## Developer commands

Not part of UCI; handy from a terminal.

| Command | |
|---|---|
| `bench` | the fixed benchmark (same as `./maiolica bench`); ends with `N nodes N nps` |
| `perft N` / `go perft N` | perft from the current position, split by move, then `Nodes searched: N` |
| `d` | print the board |
| `eval` | static eval, in centipawns from the side to move |
| `run` | infinite analysis of the start position |
| `epd [file]` | analyse every position in an EPD file (default `lct2.epd`) |
