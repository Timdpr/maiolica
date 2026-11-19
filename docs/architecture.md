# Maiolica architecture

C++20 UCI engine. Alpha-beta search on one thread by default, or several that share the hash table (Lazy SMP); 10x12 mailbox board, pawn-only bitboards, one header (`src/defs.h`). Describes the code as it is now, warts included; known issues are listed where they live.

## File map

| File | What lives there |
|---|---|
| `src/defs.h` | Everything shared: constants, enums, structs (`Board`, `Move`, `MoveList`, `HashEntry`, `HashTable`, `Undo`, `SearchInfo`), move/square macros, globals, all function decls. |
| `src/main.cpp` | `initAll`, 64MB TT, then `bench`, `console` or `replay` from the command line; otherwise prints a banner and runs `uciLoop` |
| `src/init.cpp` | Builds runtime tables: 120<->64 maps, Zobrist keys, `filesBoard`/`ranksBoard`, eval masks, MVV-LVA, the late-move reduction table (`initAll`) |
| `src/data.cpp` | Constant per-piece tables (`pieceValues`, `pieceColours`, `bigPieces`, `pieceKnight`…), `pieceChars`, `mirror64` |
| `src/board.cpp` | `resetBoard`, `parseFen`, `updateMaterialLists`, `checkBoard` (debug cross-check), `printBoard`, `mirrorBoard` |
| `src/hashkeys.cpp` | `generatePositionKey` (from scratch; make/undo update incrementally) |
| `src/attack.cpp` | `isSquareAttacked(sq, attackingSide, board)` |
| `src/movegen.cpp` | `generateAllMoves`, `generateAllCaptureMoves`, `MOVE()` macro, move scoring, `initMVVLVA`, `moveExists` |
| `src/makemove.cpp` | `makeMove`/`undoMove`, `makeNullMove`/`undoNullMove`, `HASH_*` macros, `castlePermArray` |
| `src/search.cpp` | `searchPosition` (iterative deepening), `alphaBeta`, `quiescence`, `pickNextMove`, `isRepetition`, `checkUp`, `firstLegalMove`, `scoreToUci` |
| `src/pvtable.cpp` | Transposition table: `initHashTable`, `clearHashTable`, `probeHashEntry`, `storeHashEntry`, `probePVTable`, `getPVLine`; global `hashTable` |
| `src/evaluate.cpp` | `evalPosition`, `materialDraw`, piece-square tables |
| `src/uci.cpp` | `uciLoop` and its commands, `sendLine` (all protocol output), the search thread, time budget, options |
| `src/io.cpp` | `printSquare`, `printMove` (`thread_local` buffers; `0000` for `NO_MOVE`), `printMoveList`, `parseMove` (UCI string → move via movegen) |
| `src/bitboards.cpp` | `popBit`, `countBits` (both via `<bit>`), `printBitBoard` |
| `src/perft.cpp` | `perft` (accumulates global `leafNodes`), `perftTest` (divide output) |
| `src/validate.cpp` | `squareOnBoard`, `pieceValid`… used by `ASSERT`; `debugAnalysisTest` (lct2.epd), `mirrorEvalTest` (mirror.epd) |
| `src/misc.cpp` | `getTimeMS` (steady_clock ms) |
| `src/console.cpp` | `./maiolica console` & `replay`: the terminal board (ansi/unicode, or plain), a game against the engine, san move input, pgn replay, game result detection. Self-contained; it borrows the search's output through `lineHandler` and runs nothing during uci |
| `src/bench.cpp` | `bench()`: 9 fixed FENs at fixed depths, fresh `Board`/`SearchInfo`, 16MB TT cleared per position. Total nodes is the **bench signature** |
| `tests/perft_tests.cpp` | Perft regression, tiers `smoke` (17 cases, debug/sanitiser-sized), `quick` (19, default, ~1s), `full` (8, ~20s); ctest `perft_<tier>` |
| `tests/search_tests.cpp` | A short search of five positions (~200k nodes) for debug and sanitiser builds, where the `ASSERT`s in `makeMove`, `isInCheck` and `checkBoard` run inside it, plus a check that `probePonderMove` refuses a table move that isn't legal, a table entry stored and read back at the widest value of every field, and a planted root entry that mustn't be played; ctest `search` |
| `tests/thread_tests.cpp` | The search at 1, 2, 4 and 8 threads: forced mates, legal moves, the node count, a node limit and a stop from another thread; ctest `threads` |
| `tests/eval_tests.cpp` | Eval checks: colour symmetry, the pawnless draw, the king's two tables; ctest `eval` |
| `tests/bench_signature.txt` | Expected node total; ctest `bench_signature` greps `Nodes searched  : N` from `maiolica bench` |
| `tests/microbench.cpp` | Optional Google Benchmark (`MAIOLICA_MICROBENCH=ON`, `microbench` preset): movegen, captures, make/undo, `isSquareAttacked`, eval. Indicative only; bench/perft decide |

Build/presets in `CMakeLists.txt`/`CMakePresets.json` (release, native, profile, debug, asan, tsan, microbench); `scripts/ab.sh`, `pgo.sh`, `match.sh`, `demo.sh`.

## Board representation (`Board`, defs.h)

- `pieces[120]`: 10x12 mailbox. Real squares A1=21 … H8=98; the two outer ranks and one file each side are `OFFBOARD` (100). Sliding/knight loops just step until they hit `OFFBOARD`, no bounds checks. `NO_SQ`=99.
- 64-square index (a1=0, h8=63) is used for bitboards and eval tables. Convert with `INDEX_120_TO_64`/`INDEX_64_TO_120` (`sq120ToSq64`/`sq64ToSq120`, built in `initSquare120To64`; invalid entries hold 65/120). `FILE_RANK_TO_SQUARE_INDEX(f, r)` gives the 120 index. `filesBoard`/`ranksBoard[120]` give file/rank or `OFFBOARD`; `squareOnBoard` and movegen's `SQUARE_OFFBOARD` test `filesBoard`.
- `pawns[3]`: pawn bitboards `[WHITE]`, `[BLACK]`, `[BOTH]` (64-index). The only bitboards; used by eval and `checkBoard`.
- `pieceList[13][10]` + `pieceCounts[13]`: squares of each piece type. Removal is swap-with-last (`clearPiece`), so **list order depends on move history and feeds generation order**. Initial order comes from `updateMaterialLists` scanning squares 0..119. `pieceIndex[120]` holds each occupied square's slot in its list, so `clearPiece` and `movePiece` find a piece without searching; entries for empty squares are stale and never read.
- `kingSq[2]`; `material[2]` (sum of `pieceValues`, **king's 50000 included**); `bigPieces[2]` (non-pawns, king included); `majorPieces[2]` (R, Q **and K** per `data.cpp`, despite the defs.h comment); `minorPieces[2]` (N, B).
- `side`, `enPasSq` (`NO_SQ` if none), `fiftyMove` (half-moves, from the FEN's halfmove clock), `castlingPerms` (4 bits: `wK_CA`=1, `wQ_CA`=2, `bK_CA`=4, `bQ_CA`=8).
- `ply` = depth from search root (reset by `clearForSearch`, and after each move in `parsePosition`); `historyPly` = index into `history[MAX_GAME_MOVES=2048]` of `Undo {move, castlingPerms, enPasSq, fiftyMove, positionKey, inCheck}`. The first five are saved by `makeMove` for the move played from that position; `inCheck` belongs to the position itself (see `isInCheck` below).
- `positionKey`: Zobrist key, kept incrementally.
- **Search state lives in `Board` too:** `searchHistory[13][120]` (piece × to-square), `searchKillers[2][MAX_DEPTH]` (by ply), `staticEvals[MAX_DEPTH]` (the static eval of each position on the line being searched, by ply), and what the search on this board has done: `nodes`, `selDepth`, `nullCut`, `rootMove` (the best move found at the root so far) and `threadId` (0 for the main search thread). Each search thread has a `Board` of its own, so none of these is shared. `Board` is big (74KB, mostly `history`) and copied with `memcpy` for the search thread and for the PV walk, so it must stay trivially copyable.

## Move encoding (defs.h)

A move is one `int` (25 bits used); `NO_MOVE` = 0.

| Bits | Field | Access |
|---|---|---|
| 0–6 | from (120 index) | `GET_FROM` (`& 0x7F`) |
| 7–13 | to | `GET_TO` (`>>7 & 0x7F`) |
| 14–17 | captured piece | `GET_CAPTURED` (`>>14 & 0xF`) |
| 18 | en passant | `MFLAG_EN_PASSANT` 0x40000 |
| 19 | pawn double push | `MFLAG_PAWN_START` 0x80000 |
| 20–23 | promoted-to piece | `GET_PROMOTED` (`>>20 & 0xF`), `MFLAG_PROMOTION` 0xF00000 |
| 24 | castling | `MFLAG_CASTLING` 0x1000000 |

`MFLAG_CAPTURING` 0x7C000 covers captured bits **plus** the EP bit, so EP counts as a capture (EP moves store captured = `EMPTY`). Built by `MOVE(from, to, captured, promoted, flag)` in movegen.cpp. `Move {int move; int score;}`; `MoveList {Move moves[256]; int count;}`. The layout is load-bearing: the TT stores the int.

## Move generation and ordering (movegen.cpp)

Pseudo-legal; legality is decided by `makeMove`. `generateAllMoves` order, for the side to move:
1. Pawns, in piece-list order. Per pawn: push (promotions Q, R, B, N), double push, capture +9/−9 side then +11/−11, then EP on those two squares.
2. Castling: kingside then queenside. Requires empty squares and king + transit square not attacked (`isSquareAttacked`); the destination is checked by `makeMove`.
3. Sliders B, R, Q (`slidePiecesArray`), each piece in list order, directions in `pieceDirection` order, nearest square first.
4. Non-sliders N, K (`nonSlidePiecesArray`).

`generateAllCaptureMoves` is the same minus quiet moves and castling; **quiet promotions are not generated**, so quiescence never sees them.

Scores (set at generation; `alphaBeta` then sets the TT move to 2,000,000):

| Move | Score |
|---|---|
| Capture (incl. capture-promotion) | 1,000,000 + `mVVLVAScores[victim][attacker]` = victimScore + 6 − attackerScore/100 (P..K = 100..600; PxQ 505, QxP 101) |
| En passant | 1,000,105 (= PxP) |
| Killer 0 / killer 1 at this ply | 900,000 / 800,000 |
| Other quiet (incl. quiet promotion) | `searchHistory[piece][to]`, between −16,384 and 16,384 (`HISTORY_MAX`) |

`pickNextMove` (search.cpp) is a selection step: scan from `moveNum` for strictly greater score (starting from the score at `moveNum`, since history scores can be negative), swap it into place. Ties go to the earliest index in the **current, already-swapped** list, so generation order, piece-list order and the swap pattern all decide tie-breaks (e.g. the four capture-promotions on one square score equal). **Any change to generation order, piece-list order, scores or the tie-break changes search node counts, so it changes the bench signature.**

`moveExists` (generate + try each) validates TT moves in `getPVLine`; `parseMove` matches UCI strings against generated moves.

## Make / undo (makemove.cpp)

`makeMove` saves the key, move, fifty, EP and castling into `history[historyPly]`, then incrementally updates everything through `clearPiece`/`addPiece`/`movePiece` (key via `HASH_PIECE`, material, big/major/minor counts, pawn bitboards, piece lists):
- EP removes the pawn behind `to`; castling moves the rook first (switch on `to`).
- Hashes out EP and castling, applies `castlingPerms &= castlePermArray[from] & [to]`, hashes castling back in.
- Capture → `clearPiece(to)`, fifty = 0. Pawn move → fifty = 0; a double push sets `enPasSq` and hashes it only when an enemy pawn stands beside the pushed pawn, so a square nobody can use doesn't give the same position a second key. `parseFen` applies the same rule. The pawn beside may be pinned: that isn't checked.
- Moves the piece, swaps in the promoted piece, updates `kingSq`, flips side and `HASH_SIDE`.
- **Legality:** if the mover's king is now attacked, `undoMove` and return false. Every caller must handle the false.
  - King moves (castling included), en passant, and any move played while in check get the full `isSquareAttacked` on the king.
  - Every other move can only expose the king by leaving a line it shared with it, so `kingExposed` looks along that one line from the king (`lineStep` gives the direction from the difference between the two squares) for an enemy slider.
  - An illegal move is still made and then undone, not refused up front: making and undoing a capture moves the captured piece to the end of its piece list, and generation order depends on that.

`isInCheck(board)` answers "is the side to move in check?" once per position: the answer is kept in `history[historyPly].inCheck`, which `makeMove`, `makeNullMove` and `resetBoard` set to `CHECK_UNKNOWN` for the position they create. `makeMove` and `alphaBeta` (for the check extension) both ask it. The first time, it is worked out from the move that led to the position: an ordinary move can only give check with the piece that moved (`attacksSquare`) or by uncovering a line through the square it left (`kingExposed`), so only those two are looked at. The full `isSquareAttacked` on the king is kept for a position with no move before it, after a null move, and after en passant or castling, which shift a second piece. A debug build checks every answer against the full test. Anything else that changes the position without going through those must reset it.

`undoMove` reverses it incrementally (re-hashing, not restoring the key from `history`; the stored key is only read by `isRepetition`). `makeNullMove`/`undoNullMove` flip side, clear EP (hashed), bump `ply`/`historyPly`; fifty and castling are untouched.

Zobrist: `pieceKeys[13][120]` (row `EMPTY` doubles as the EP-square key), `sideKey` (in when white to move), `castleKeys[16]` indexed by the perms nibble.

## Search (search.cpp)

`searchPosition`: `clearForSearch` (zero history/killers, `currentAge++`, `ply = 0`, reset the board's counts and `rootMove`; `stopped` is reset by the caller, before the thread starts), then it starts the helper threads if `info.threads > 1` (see "Threads" below) and runs `deepen`: depths 1..`info.depth`. Depths 1 to 3 get the full window (±`ALPHABETA_BOUND` 30000), and so does a depth that follows a mate score. From depth 4 (`ASPIRATION_MIN_DEPTH`) the window is an aspiration window: the last depth's score ± 50 (`ASPIRATION_WINDOW`). A score at or below alpha (a fail low) moves alpha down by the step and the depth is searched again; a score at or above beta (a fail high) moves beta up by it. The step starts at 50 and doubles after every miss, and the bound that didn't fail stays where it was, so the failed side sits 100, 200, 400… from the last score until it reaches the full window, which can't fail. Nothing is printed for a try that failed. A stopped iteration is discarded, except that a move which failed high before the stop is kept as the best move: it has just been shown to beat the old one. When the score lands inside the window, `getPVLine` fills a local `pvArray`, starting from `board.rootMove`, and it sends `info depth … seldepth … score cp|mate … nodes … nps … hashfull … time … pv …`. After a finished depth, if a clock is running and the soft limit has passed (`timeSet && now > softStopTime`), the loop ends there and no further depth is started. `bestmove` is `pvArray[0]`, with `pvArray[1]` as the ponder move. When the PV is one move long (the table had no reply, or the move repeats a position), `probePonderMove` asks the table for a legal reply to it, on a copy of the board. If no iteration finished it falls back to `firstLegalMove` (`0000` if mated or stalemated); that runs after the search, because making and undoing a move can reorder the piece lists. In `infinite` or `ponder` mode it then waits for `stopped`/`ponderhit` before sending `bestmove`.

`alphaBeta(alpha, beta, depth, board, info, table, nullMoveAllowed)`, negamax, in this order:
1. `depth == 0` and not in check (`isInCheck`) → `quiescence` (which counts the node). A position in check carries on instead, and step 4 gives it a ply: quiescence would stand pat on a position that can't stand still, and can't see a checkmate. So quiescence never starts from a position in check.
2. `checkUp` when `(board.nodes & 2047) == 0`; `board.nodes++`.
3. Repetition or `fiftyMove >= 100` → 0 (only at `ply > 0`, so the root always searches its moves). `ply > MAX_DEPTH-1` → eval.
4. In check → `depth++` (before the TT probe, so stored depths include the extension). At `depth == 0` this is what keeps the position out of quiescence (step 1).
5. `probeHashEntry`: a hit returns its score; the TT move is returned even when too shallow. At the root, a hit whose score beats alpha also sets `board.rootMove` to the entry's move.
6. The static eval, and the two futility prunings that trust it near the leaves. `evalPosition` is called once, at every position that isn't in check, and the result is kept in `board.staticEvals[ply]` (`NO_EVAL` for a position in check), so that positions further down the line can compare theirs with it. Both prunings need: not in check, `ply > 0`, a null window (`beta - alpha == 1`, so never on the main line) and `depth <= 6` (`FUTILITY_MAX_DEPTH`).
   - Reverse futility, at `depth <= 5` (`RFP_MAX_DEPTH`) when beta is not a mate score: an eval at least `120 × depth` (`RFP_MARGIN`) above beta → return beta, with no move tried and nothing stored in the TT.
   - Futility, when alpha is not a mate score and `bigPieces[side] > 1`: if eval + `75 + 125 × depth` (200 at depth 1, 825 at depth 6) is still at or below alpha, the node is marked futile. In the move loop (step 8), once one legal move has been searched, every quiet move (no capture, no promotion) is then skipped without being made. That includes killers and quiet moves that give check; skipped moves aren't added to the list of non-captures tried.
   - Late move pruning shares those conditions bar the eval (not in check, `ply > 0`, a null window, `depth <= 6`, `bigPieces[side] > 1`): there the node gets a limit of `3 + depth²` moves (`LMP_BASE`: 4 at depth 1, 7 at depth 2, then 12, 19, 28, 39), used in step 8.
7. Null move if allowed, not in check, `ply > 0`, `bigPieces[side] > 1` (king counts, so ≥1 real piece) and `depth >= 4`: search `depth-4` (R=3) with window (−beta, −beta+1) and null disabled; `score >= beta` and not a mate score → return beta (not stored in the TT).
8. Generate, give the TT move 2,000,000, loop with `pickNextMove`. PVS: the first move with a full window; once alpha has been raised, null window then re-search if `alpha < score < beta`.
   Late move pruning, before a move is made: once the node's limit of legal moves have been searched (step 6), the next quiet move that isn't a killer sets `skipQuiets`, and it and every quiet move after it are skipped unmade. Not while the best score so far is a mated score. Captures and killers are sorted ahead of the other quiet moves, so by then the only moves left that are still searched are promotions without a capture; they are taken in the order they were generated, as `pickNextMove` is no longer called. Quiet moves that give check are skipped with the rest, and skipped moves aren't added to the list of non-captures tried.
   Late move reduction, before that: from the 5th legal move on (`LMR_FULL_MOVES` 4), at `depth >= 3` (`LMR_MIN_DEPTH`), a quiet move (no capture, no promotion) that isn't a killer at this ply is first searched at `depth-1-r` with window (−alpha−1, −alpha). `r` comes from `lmrReductions[depth][legal]`, filled at start-up by `initLmrReductions` as max(1, ⌊0.75 + ln(depth) × ln(legal) / 2.25⌋), and is capped at `depth-2` so a ply is always left: 1 for the 5th move at depth 3, 2 for the 10th at depth 6, 4 for the 30th at depth 12. Not when in check, nor when the move gives check. Only a score above alpha sends it on to the PVS search at full depth; otherwise the reduced score stands.
9. Beta cutoff (at the root, the move becomes `board.rootMove`; so does a move that raises alpha there): non-captures (by `MFLAG_CAPTURING`) shift into killers at this ply; store `HF_BETA` with beta; return beta. A non-capture that cuts off also has its history score raised by `historyBonus(depth)` = min(16 × depth², 1,600), and every non-capture searched before it at this node is lowered by the same amount. Alpha raised without a cutoff: a non-capture gets the same bonus, and nothing is lowered. `updateHistory` applies each change as `entry += bonus − entry × |bonus| / HISTORY_MAX`, which keeps scores within ±16,384 (far below the killers) and lets new results outweigh old ones.
10. No legal moves: `-ALPHABETA_BOUND + ply` if in check, else 0. Store `HF_EXACT` (bestScore) if alpha rose, else `HF_ALPHA` (alpha, with the best fail-low move).

`quiescence`: checkUp/nodes/`selDepth`, repetition/fifty (only at `ply > 0`), max-ply eval, stand pat (`eval >= beta` → beta, raise alpha), then `generateAllCaptureMoves` with the same ordering. No TT, no delta pruning. It is never entered in check (step 1 of `alphaBeta`), but a capture inside it can give check, and the side in check there still stands pat or tries captures only: no check evasions.

`isRepetition` scans `history[max(0, historyPly - fiftyMove) .. historyPly-2]` for the current key; one earlier occurrence counts as a draw. `checkUp` checks the clock (`timeSet && now > stopTime`, the hard limit) and `nodeLimit`, on the main thread only (a helper adds 2048 to `info.helperNodes` there and returns); the soft limit (`softStopTime`) is looked at in `searchPosition` after each finished depth, and ends the search there; a GUI `stop` reaches the search via the atomic `info.stopped`, read with relaxed ordering in the move loops. On `stopped`, both searches return 0 after undoing the move.

Mate scores: `IS_MATE` = 30000 − 64. Anything beyond it is printed as `score mate N` (`scoreToUci`).

## Transposition table (pvtable.cpp)

- `HashEntry {std::atomic<U64> keyXorData, data}`: 16 bytes, two 64-bit words. `data` packs the move (bits 0–24, the move int as it is), the score + 32768 (16 bits from bit 25), the depth (7 bits from 41), the flags (2 bits from 48) and the age (14 bits from 50). The other word is the position key xored with `data`. `numEntries = MB*2^20 / sizeof(HashEntry) - 2`. Index = `positionKey % numEntries`, one entry per slot. The size and the `-2` change indices, so they change node counts.
- **Reading an entry** (`readEntry`): load both words once, and accept the entry only if `keyXorData ^ data == positionKey`. Every field then comes from that one copy of `data`. This is the lockless scheme of Hyatt and Mann: if another thread wrote the slot between the two loads, the words come from different entries, the test fails and the entry is treated as not there. With one thread the test is the old full-key comparison. An empty slot has `data == 0`, which no stored entry has (the score field is never 0). Loads and stores are `memory_order_relaxed`: on the processors we build for an atomic 64-bit word costs what a plain one does.
- Flags: `HF_ALPHA` (upper bound), `HF_BETA` (lower bound), `HF_EXACT`. Probe needs `entry.depth >= depth`; ALPHA hits only if `score <= alpha` (returns alpha), BETA only if `score >= beta` (returns beta).
- Replace if the slot is empty, the entry's age isn't the table's (`currentAge`, kept to 14 bits), or `entry.depth <= depth`. Age is bumped per search, reset to 0 by `clearHashTable` (`ucinewgame`, bench). "Isn't" was "older than" before the age was packed; they differ only for an entry left untouched for exactly 16,384 searches without a new game, which is then kept for one search more.
- Mate scores are stored relative to the node (`±ply` on store, reversed on probe).
- `getPVLine` copies the board and walks moves from the root on the copy, checking each with `moveExists` and making it, up to `depth` moves. The first is the move it is given (the search's own `rootMove`); the rest are `probePVTable`'s. The root's entry isn't trusted for the move to play, because it isn't always the search's own: the table refuses an entry when a deeper one from the same search is in the slot, and with several threads that one is another thread's, which may be an upper bound whose move means little. It stops after the first move that repeats a position or reaches the fifty-move limit: the search scores that as a draw and looks no further, so later entries belong to other lines. The search's own board is `const` here. Making and undoing moves reorders the piece lists, so a walk on that board would change later searches by how far it went.

## Evaluation (evaluate.cpp)

White-relative score, negated for black at the end. Tables are a1-first from white's view; black looks up `MIRROR_64(sq64)`.
- Material difference (kings cancel) + PSTs for P, N, B, R (no queen table).
- Pawns: isolated −10 (`isolatedPawnMask`: adjacent files); passed bonus `pawnPassed[rank]` {0,5,10,20,35,60,100,200} (`whitePassedMask`/`blackPassedMask`: own and adjacent files ahead; black uses `7 - rank`).
- Rook open/semi-open file +10/+5, queen +5/+3 (`fileBitMask` vs `pawns[BOTH]`/own pawns).
- King: `kingEndgame` if the *opponent's* `material <= ENDGAME_MATERIAL` (R + 2N + 2P = 1400, plus the king's 50000, since `material` counts the king), else `kingOpening`. Bishop pair +30.
- `materialDraw` (from Sjeng): with no pawns on the board, returns 0 for drawish minor/rook-only material.

## Threads (search.cpp, uci.cpp)

Protocol details are in [uci.md](uci.md).

**Search threads (Lazy SMP).** With `info.threads` at 1 (the default; the `Threads` option sets it) there is one search thread and nothing below applies. Above 1:
- `searchPosition` runs on the main search thread. After `clearForSearch` it copies its board once per helper (on the heap), gives each copy a `threadId`, and starts a `std::thread` on each running `deepen`: the same loop, depths and aspiration window as the main thread, to the same depth limit. No thread is told what to search. They drift apart because each has its own killers and history scores and finds different things in the table, and they help each other only through the table: a depth one thread has finished is found there by the next thread to reach it.
- **The main thread decides everything.** It alone looks at the clock (hard limit in `checkUp`, soft limit after each depth) and the node limit, prints `info` lines and `bestmove`, and plays its own `rootMove`. A helper prints nothing and stops only when `stopped` is set.
- **Stopping:** when `deepen` returns on the main thread (and after the wait for `stop` or `ponderhit` under `go infinite` and `go ponder`, during which the helpers search on), it sets `stopped`, joins every helper and only then sends `bestmove`. So nothing is writing to the table once the gui has its move. A stopped search stores nothing and its result is thrown away, as with one thread.
- **Node counts:** each thread counts in its own `Board`. A helper adds 2048 to the atomic `info.helperNodes` each time it has done that many, which is what the node limit and the `nodes`/`nps` of an `info` line use; `info.nodes` at the end is the exact sum. `seldepth` is the main thread's.
- **Shared, and how:** the table's entries (atomics, checked as above); `stopped`, `ponder`, `timeSet`, the two stop times and `helperNodes` (atomics); `info.depth`, `nodeLimit`, `infinite`, `startTime`, the table's size and age (written before the helpers start, only read while they run). Everything else a thread writes is in its own `Board` or on its stack.
- Threads are started for every search and joined at its end. That costs about 0.1 ms for seven helpers, and leaves no state between searches.
- `bench`, the console and the tests build their own `SearchInfo`, where `threads` is 1, so they never pick the option up. `epd` uses the UCI's and does.

**Input and search.**
- The input thread owns one `std::thread searchThread` (the main search thread). `stopSearch()` sets `stopped`, clears `ponder` and joins; it runs before every `go`, `setoption`, `ucinewgame`, the dev commands that search or use the table (`bench`, `perft`, `epd`) and on exit, and is a no-op when idle. `d` and `eval` only read the gui's board, so they don't stop it. `position` doesn't stop a search, since the search works on its own copy.
- `go` sets every `SearchInfo` field on the input thread (including `stopped = false`) *before* launching, then hands the thread a heap copy of the board (`std::make_unique<Board>`), so the gui can carry on.
- Shared between the input and search threads: `stopped`, `ponder`, `timeSet`, `softStopTime`, `stopTime` (atomics; `ponderhit` writes the two stop times, then `timeSet`). Everything else is written before launch or by the search threads only.
- All protocol output goes through `sendLine` (printf-style, one mutex-guarded write + flush per line), so `readyok` can't land mid `info` line. If `lineHandler` is set (only by the console), lines go to it instead, so the console can show the search its own way. `printMove` uses `thread_local` buffers.
- Global TT `hashTable`, reallocated by `setoption name Hash` after stopping the search.

## Reproducibility

Zobrist keys come from a fixed-seed splitmix64 (`rand64` in init.cpp, seed `"MAIOLICA"`), filled in order: `pieceKeys`, `sideKey`, `castleKeys`. Node counts are therefore identical across runs, platforms and Debug/Release, with one search thread. With more, which thread writes a table entry first is down to timing, so two runs of the same search differ in nodes and sometimes in the move; `bench` always uses one. **A refactor must leave the bench signature unchanged.** A deliberate behaviour change updates `tests/bench_signature.txt` and puts `Bench: N` in the commit message.
