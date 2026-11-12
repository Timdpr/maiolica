# Maiolica architecture

C++20 UCI engine. Single-threaded search, 10x12 mailbox board, pawn-only bitboards, one header (`src/defs.h`). Describes the code as it is now, warts included; known issues are listed where they live.

## File map

| File | What lives there |
|---|---|
| `src/defs.h` | Everything shared: constants, enums, structs (`Board`, `Move`, `MoveList`, `HashEntry`, `HashTable`, `Undo`, `SearchInfo`, `SearchThreadData`), move/square macros, globals, all function decls. (Its "// board.cpp" comment above `evalPosition` is wrong: that lives in evaluate.cpp.) |
| `src/main.cpp` | `initAll`, 64MB TT, `maiolica bench` CLI arg; otherwise waits for `uci` (or `quit`) then enters `uciLoop` |
| `src/init.cpp` | Builds runtime tables: 120<->64 maps, set/clear masks, Zobrist keys, `filesBoard`/`ranksBoard`, eval masks, MVV-LVA (`initAll`) |
| `src/data.cpp` | Constant per-piece tables (`pieceValues`, `pieceColours`, `bigPieces`, `pieceKnight`…), `pieceChars`, `mirror64` |
| `src/board.cpp` | `resetBoard`, `parseFen`, `updateMaterialLists`, `checkBoard` (debug cross-check), `printBoard`, `mirrorBoard` |
| `src/hashkeys.cpp` | `generatePositionKey` (from scratch; make/undo update incrementally) |
| `src/attack.cpp` | `isSquareAttacked(sq, attackingSide, board)` |
| `src/movegen.cpp` | `generateAllMoves`, `generateAllCaptureMoves`, `MOVE()` macro, move scoring, `initMVVLVA`, `moveExists` |
| `src/makemove.cpp` | `makeMove`/`undoMove`, `makeNullMove`/`undoNullMove`, `HASH_*` macros, `castlePermArray` |
| `src/search.cpp` | `searchPosition` (iterative deepening), `alphaBeta`, `quiescence`, `pickNextMove`, `isRepetition`, `checkUp`, `searchPositionThread` |
| `src/pvtable.cpp` | Transposition table: `initHashTable`, `clearHashTable`, `probeHashEntry`, `storeHashEntry`, `probePVTable`, `getPVLine`; global `hashTable[1]` |
| `src/evaluate.cpp` | `evalPosition`, `materialDraw`, piece-square tables |
| `src/uci.cpp` | `uciLoop`, `parseGo`, `parsePosition`, search thread launch/join |
| `src/io.cpp` | `printSquare`, `printMove` (static buffers), `printMoveList`, `parseMove` (UCI string → move via movegen) |
| `src/bitboards.cpp` | `popBit` (De Bruijn-style fold), `countBits`, `printBitBoard` |
| `src/perft.cpp` | `perft` (accumulates global `leafNodes`), `perftTest` (divide output) |
| `src/validate.cpp` | `squareOnBoard`, `pieceValid`… used by `ASSERT`; `debugAnalysisTest` (lct2.epd), `mirrorEvalTest` (mirror.epd) |
| `src/misc.cpp` | `getTimeMS` (steady_clock ms) |
| `src/bench.cpp` | `bench()`: 9 fixed FENs at fixed depths, fresh `Board`/`SearchInfo`, 16MB TT cleared per position. Total nodes is the **bench signature** |
| `tests/perft_tests.cpp` | Perft regression, tiers `smoke` (8 cases, debug/sanitiser-sized), `quick` (19, default, ~1s), `full` (6, ~20s); ctest `perft_<tier>` |
| `tests/bench_signature.txt` | Expected node total (currently `4527030`); ctest `bench_signature` greps `Nodes searched  : N` from `maiolica bench` |
| `tests/microbench.cpp` | Optional Google Benchmark (`MAIOLICA_MICROBENCH=ON`, `microbench` preset): movegen, captures, make/undo, `isSquareAttacked`, eval. Indicative only; bench/perft decide |

Build/presets in `CMakeLists.txt`/`CMakePresets.json` (release, native, profile, debug, asan, tsan, microbench); `scripts/ab.sh`, `pgo.sh`, `match.sh`.

## Board representation (`Board`, defs.h)

- `pieces[120]`: 10x12 mailbox. Real squares A1=21 … H8=98; the two outer ranks and one file each side are `OFFBOARD` (100). Sliding/knight loops just step until they hit `OFFBOARD`, no bounds checks. `NO_SQ`=99.
- 64-square index (a1=0, h8=63) is used for bitboards and eval tables. Convert with `INDEX_120_TO_64`/`INDEX_64_TO_120` (`sq120ToSq64`/`sq64ToSq120`, built in `initSquare120To64`; invalid entries hold 65/120). `FILE_RANK_TO_SQUARE_INDEX(f, r)` gives the 120 index. `filesBoard`/`ranksBoard[120]` give file/rank or `OFFBOARD`; `squareOnBoard` and movegen's `SQUARE_OFFBOARD` test `filesBoard`.
- `pawns[3]`: pawn bitboards `[WHITE]`, `[BLACK]`, `[BOTH]` (64-index). The only bitboards; used by eval and `checkBoard`.
- `pieceList[13][10]` + `pieceCounts[13]`: squares of each piece type. Removal is swap-with-last (`clearPiece`), so **list order depends on move history and feeds generation order**. Initial order comes from `updateMaterialLists` scanning squares 0..119.
- `kingSq[2]`; `material[2]` (sum of `pieceValues`, **king's 50000 included**); `bigPieces[2]` (non-pawns, king included); `majorPieces[2]` (R, Q **and K** per `data.cpp`, despite the defs.h comment); `minorPieces[2]` (N, B).
- `side`, `enPasSq` (`NO_SQ` if none), `fiftyMove` (half-moves; FEN halfmove clock is not parsed, starts at 0), `castlingPerms` (4 bits: `wK_CA`=1, `wQ_CA`=2, `bK_CA`=4, `bQ_CA`=8).
- `ply` = depth from search root (reset by `clearForSearch`, and after each move in `parsePosition`); `historyPly` = index into `history[MAX_GAME_MOVES=2048]` of `Undo {move, castlingPerms, enPasSq, fiftyMove, positionKey}`.
- `positionKey`: Zobrist key, kept incrementally.
- **Search state lives in `Board` too:** `pvArray[MAX_DEPTH]`, `searchHistory[13][120]` (piece × to-square), `searchKillers[2][MAX_DEPTH]` (by ply). `Board` is big (57KB, mostly `history`) and copied with `memcpy` for the search thread, so it must stay trivially copyable.

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

`MFLAG_CAPTURING` 0x7C000 covers captured bits **plus** the EP bit, so EP counts as a capture (EP moves store captured = `EMPTY`). Built by `MOVE(from, to, captured, promoted, flag)` in movegen.cpp. `Move {int move; int score;}`; `MoveList {Move moves[256]; int count;}`. The layout is load-bearing: the TT stores the int, and `FOLD_DATA` shifts it into bits 25+ of a 64-bit word. (The bit diagram comment in defs.h has a typo on the pawn-start line; the masks are right.)

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
| Other quiet (incl. quiet promotion) | `searchHistory[piece][to]` |

`pickNextMove` (search.cpp) is a selection step: scan from `moveNum` for strictly greater score (starting from 0), swap it into place. Ties go to the earliest index in the **current, already-swapped** list, so generation order, piece-list order and the swap pattern all decide tie-breaks (e.g. the four capture-promotions on one square score equal). **Any change to generation order, piece-list order, scores or the tie-break changes search node counts, so it changes the bench signature.**

`moveExists` (generate + try each) validates TT moves in `getPVLine`; `parseMove` matches UCI strings against generated moves.

## Make / undo (makemove.cpp)

`makeMove` saves the key, move, fifty, EP and castling into `history[historyPly]`, then incrementally updates everything through `clearPiece`/`addPiece`/`movePiece` (key via `HASH_PIECE`, material, big/major/minor counts, pawn bitboards, piece lists):
- EP removes the pawn behind `to`; castling moves the rook first (switch on `to`).
- Hashes out EP and castling, applies `castlingPerms &= castlePermArray[from] & [to]`, hashes castling back in.
- Capture → `clearPiece(to)`, fifty = 0. Pawn move → fifty = 0; a double push always sets `enPasSq` and hashes it, capturable or not (B12).
- Moves the piece, swaps in the promoted piece, updates `kingSq`, flips side and `HASH_SIDE`.
- **Legality:** if the mover's king is now attacked, `undoMove` and return false. Every caller must handle the false.

`undoMove` reverses it incrementally (re-hashing, not restoring the key from `history`; the stored key is only read by `isRepetition`). `makeNullMove`/`undoNullMove` flip side, clear EP (hashed), bump `ply`/`historyPly`; fifty and castling are untouched.

Zobrist: `pieceKeys[13][120]` (row `EMPTY` doubles as the EP-square key), `sideKey` (in when white to move), `castleKeys[16]` indexed by the perms nibble.

## Search (search.cpp)

`searchPosition`: `clearForSearch` (zero history/killers, `currentAge++`, `ply = 0`, reset `stopped`/`nodes`/fail-high stats), then depths 1..`info->depth` with a full window (±`ALPHABETA_BOUND` 30000), no aspiration. A stopped iteration is discarded; otherwise `getPVLine` fills `pvArray`, prints `info score cp … depth … nodes … time … pv …`. Ends with `bestmove`, initialised from `pvArray[0]`. If no iteration completes, that's never written, so `printMove(NO_MOVE)` prints garbage (known issue).

`alphaBeta(alpha, beta, depth, board, info, table, nullMoveAllowed)`, negamax, in this order:
1. `depth == 0` → `quiescence` (which counts the node).
2. `checkUp` when `(nodes & 2047) == 0`; `nodes++`.
3. Repetition or `fiftyMove >= 100` → 0 (no `board.ply` guard here, unlike quiescence, so a repetition at the root scores 0 and yields no bestmove: known issue). `ply > MAX_DEPTH-1` → eval.
4. In check → `depth++` (before the TT probe, so stored depths include the extension).
5. `probeHashEntry`: a hit returns its score; the TT move is returned even when too shallow.
6. Null move if allowed, not in check, `ply > 0`, `bigPieces[side] > 1` (king counts, so ≥1 real piece) and `depth >= 4`: search `depth-4` (R=3) with window (−beta, −beta+1) and null disabled; `score >= beta` and not a mate score → return beta (not stored in the TT).
7. Generate, give the TT move 2,000,000, loop with `pickNextMove`. PVS: the first move with a full window; once alpha has been raised, null window then re-search if `alpha < score < beta`.
8. Beta cutoff: non-captures (by `MFLAG_CAPTURING`) shift into killers at this ply; store `HF_BETA` with beta; return beta. Alpha raised: quiet moves get `searchHistory[piece][to] += depth` (only on alpha raises, not on cutoffs).
9. No legal moves: `-ALPHABETA_BOUND + ply` if in check, else 0. Store `HF_EXACT` (bestScore) if alpha rose, else `HF_ALPHA` (alpha, with the best fail-low move).

`quiescence`: checkUp/nodes, repetition/fifty (only at `ply > 0`), max-ply eval, stand pat (`eval >= beta` → beta, raise alpha), then `generateAllCaptureMoves` with the same ordering. No TT, no check evasions, no delta pruning.

`isRepetition` scans `history[historyPly - fiftyMove .. historyPly-2]` for the current key; one earlier occurrence counts as a draw. `checkUp` only checks the clock (`timeSet && now > stopTime`); a GUI `stop` reaches the search via `info->stopped`. On `stopped`, both searches return 0 after undoing the move.

Mate scores: `IS_MATE` = 30000 − 64. Printed as `cp` (no `score mate` yet).

## Transposition table (pvtable.cpp)

- `HashEntry {U64 positionKey; int move, score, depth, flags, age}`: 32 bytes. `numEntries = MB*2^20 / sizeof(HashEntry) - 2`. Index = `positionKey % numEntries`, one entry per slot, full key compared. The size and the `-2` change indices, so they change node counts.
- Flags: `HF_ALPHA` (upper bound), `HF_BETA` (lower bound), `HF_EXACT`. Probe needs `entry.depth >= depth`; ALPHA hits only if `score <= alpha` (returns alpha), BETA only if `score >= beta` (returns beta).
- Replace if the slot is empty (key 0), `age < currentAge`, or `entry.depth <= depth`. Age is bumped per search, reset to 0 by `clearHashTable` (`ucinewgame`, bench).
- Mate scores are stored relative to the node (`±ply` on store, reversed on probe).
- `getPVLine` walks `probePVTable` moves from the root, checking each with `moveExists`, making it, up to `depth` moves; then undoes back to `ply == 0`.
- `FOLD_DATA`/`EXTRACT_*` pack score+`INF_BOUND` (16 bits), depth (6), flags (2, at bit 23) and move (bit 25+) into one U64. They're only used by the dead `dataCheck`/`tempHashTest`, and are kept for a future lockless (Lazy SMP) TT.

## Evaluation (evaluate.cpp)

White-relative score, negated for black at the end. Tables are a1-first from white's view; black looks up `MIRROR_64(sq64)`.
- Material difference (kings cancel) + PSTs for P, N, B, R (no queen table).
- Pawns: isolated −10 (`isolatedPawnMask`: adjacent files); passed bonus `pawnPassed[rank]` {0,5,10,20,35,60,100,200} (`whitePassedMask`/`blackPassedMask`: own and adjacent files ahead; black uses `7 - rank`).
- Rook open/semi-open file +10/+5, queen +5/+3 (`fileBitMask` vs `pawns[BOTH]`/own pawns).
- King: `kingEndgame` if the *opponent's* `material <= ENDGAME_MATERIAL` (R + 2N + 2P = 1400), else `kingOpening`. Bishop pair +30.
- `materialDraw` (from Sjeng): returns 0 for drawish minor/rook-only material.

Known issues, left as they are until a fix passes an SPRT (they change play):
- **E1:** the `materialDraw` gate reads `pieceCounts[wP] && !pieceCounts[bP]` (meant to be `!wP && !bP`), so it fires only when white has pawns and black has none. This makes eval colour-asymmetric.
- **E2:** `material` includes the king's 50000, so the 1400 threshold is never met and `kingEndgame` is never used.

## Threading and UCI (uci.cpp)

Minimal and fragile; due a rewrite, so don't build on it.
- `go` parses time controls (`time/movesToGo - 50 + inc`, `movetime` sets movesToGo = 1), then starts the global `std::thread MainSearchThread` running `searchPositionThread`. That gets a `new SearchThreadData{&board, info, table}` (never freed) and `memcpy`s the board inside the thread.
- `stop`/`quit` set `info->stopped` (a plain `int`, a data race) and `join()`. `go` doesn't join a previous thread.
- The search thread prints `info`/`bestmove` itself. Global TT `hashTable`; `setoption name Hash value N` (4..2048) reallocates it.
- Known issues:
  - a second `go` without `stop` aborts (the thread is reassigned while joinable), so GUI games die at move 2;
  - an idle `stop` throws from `join()`;
  - `go infinite` then an immediate `stop` deadlocks;
  - EOF spins at 100% CPU;
  - `debug` runs an EPD test and leaves the loop;
  - position parsing uses fixed offsets, so extra spaces corrupt the board;
  - non-UCI output (board dumps, a `time:` line);
  - `Hash` isn't advertised;
  - mates print as `cp`;
  - time management can go negative or overspend with large increments.

## Reproducibility

Zobrist keys come from a fixed-seed splitmix64 (`rand64` in init.cpp, seed `"MAIOLICA"`), filled in order: `pieceKeys`, `sideKey`, `castleKeys`. Node counts are therefore identical across runs, platforms and Debug/Release. **A refactor must leave the bench signature unchanged.** A deliberate behaviour change updates `tests/bench_signature.txt` and puts `Bench: N` in the commit message.
