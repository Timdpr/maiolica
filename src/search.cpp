#include "defs.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

/// Check if time (or the node limit) is up. Neither applies while pondering: that only ends when the gui says so.
/// Only the main thread decides. A helper, called every 2048 of its nodes, just adds those to the shared count
static void checkUp(const Board& board, SearchInfo& info) {
    if (board.threadId != 0) {
        if (board.nodes != 0) {
            info.helperNodes.fetch_add(2048, std::memory_order_relaxed);
        }
        return;
    }
    long long nodes = board.nodes + info.helperNodes.load(std::memory_order_relaxed);
    if ((info.timeSet && getTimeMS() > info.stopTime) || (info.nodeLimit && nodes >= info.nodeLimit && !info.ponder)) {
        info.stopped = true;
    }
}

/// Swap the move at 'moveNum' index with the 'best' move in the moveList
static void pickNextMove(int moveNum, MoveList& moveList) {
    int bestScore = moveList.moves[moveNum].score; // start from the first candidate, as history scores can be negative
    int bestMoveNum = moveNum;
    for (int i = moveNum + 1; i < moveList.count; ++i) { // find move with highest score
        if (moveList.moves[i].score > bestScore) {
            bestScore = moveList.moves[i].score;
            bestMoveNum = i;
        }
    }
    ASSERT(moveNum >= 0 && moveNum < moveList.count)
    ASSERT(bestMoveNum >= 0 && bestMoveNum < moveList.count)
    ASSERT(bestMoveNum >= moveNum)
    // and swap it with move at 'moveNum'
    Move tempMove = moveList.moves[moveNum];
    moveList.moves[moveNum] = moveList.moves[bestMoveNum];
    moveList.moves[bestMoveNum] = tempMove;
}

int isRepetition(const Board& board) {
    // start at current ply minus how long since the fifty move rule was reset, and look at hash keys
    for (int i = std::max(0, board.historyPly - board.fiftyMove); i < board.historyPly-1; ++i) { // the fen's clock can predate our history // todo: quicker way to do this in yt comments?
        ASSERT(i >= 0 && i < MAX_GAME_MOVES)
        // if current hash key matches, then this must be a repetition!
        if (board.positionKey == board.history[i].positionKey) {
            return true;
        }
    }
    return false;
}

/// Clear relevant arrays before starting to search and set up search info
static void clearForSearch(Board& board, SearchInfo& info, HashTable& table) {
    for (auto& piece : board.searchHistory) {
        for (int& square : piece) {
            square = 0;
        }
    }
    for (auto& killerMove : board.searchKillers) {
        for (int& depth : killerMove) {
            depth = 0;
        }
    }
    table.currentAge++;

    board.ply = 0;
    board.nodes = 0;
    board.selDepth = 0;
    board.nullCut = 0;
    board.rootMove = NO_MOVE;
    board.threadId = 0;

    info.nodes = 0;
    info.selDepth = 0;
    info.helperNodes = 0;
}

static int quiescence(int alpha, int beta, Board& board, SearchInfo& info) {
    ASSERT(checkBoard(board))
    ASSERT(beta > alpha)
    // Check time remaining:
    if ((board.nodes & 2047) == 0) {
        checkUp(board, info);
    }
    board.nodes++;
    board.selDepth = std::max(board.selDepth, board.ply);

    if ((isRepetition(board) || board.fiftyMove >= 100) && board.ply) { // no repetition or 50 move AND we have at least one move
        return 0;
    }
    if (board.ply > MAX_DEPTH-1) { // at max depth allowed!
        return evalPosition(board);
    }
    // immediately get score
    int score = evalPosition(board);
    // if score is better than beta, assume that our position is better & return (?)
    if (score >= beta) {
        return beta;
    }
    //
    if (score > alpha) {
        alpha = score;
    }

    MoveList moveList;
    generateAllCaptureMoves(board, moveList); // note - only capture moves!

    for (int moveNum = 0; moveNum < moveList.count; ++moveNum) {
        // order the moves
        pickNextMove(moveNum, moveList);
        // a capture that loses material once the other side has taken back (queen takes a defended pawn) isn't tried
        if (!seeAtLeast(board, moveList.moves[moveNum].move, 0)) {
            continue;
        }
        // skip if it is not legal
        if (!makeMove(board, moveList.moves[moveNum].move)) {
            continue;
        }
        // negamax, so flip it all...
        score = -quiescence(-beta, -alpha, board, info);
        undoMove(board);

        if (info.stopped.load(std::memory_order_relaxed)) { // if we have been told to stop, stop, but AFTER TAKEMOVE!!
            return 0;
        }

        if (score > alpha) {     // if score is greater than alpha...
            if (score >= beta) { // if we have beta cutoff, return beta...
                return beta;
            }
            alpha = score;       // otherwise, update score and bestMove
//            bestMove = moveList.moves[moveNum].move;
        }
    }
    // todo: should we store the best move in the hash table (if alpha != the   alpha given to the function) here?
    return alpha;
}

// Late move reductions: the first few moves at a node always get a full search, and so does everything near the leaves
constexpr int LMR_FULL_MOVES = 4;
constexpr int LMR_MIN_DEPTH = 3;
constexpr int LMR_MAX_MOVES = 64;
static int lmrReductions[MAX_DEPTH][LMR_MAX_MOVES]; // plies to reduce by, indexed by depth left & how many legal moves in we are

/// Fill the reduction table: the deeper there is still to go and the later the move, the more it is reduced
void initLmrReductions() {
    for (int depth = 1; depth < MAX_DEPTH; ++depth) {
        for (int moves = 1; moves < LMR_MAX_MOVES; ++moves) {
            lmrReductions[depth][moves] = std::max(1, int(0.75 + std::log(depth) * std::log(moves) / 2.25));
        }
    }
}

// Reverse futility pruning: how close to the leaves it applies, & how far above beta the eval must be per ply of depth left
constexpr int RFP_MAX_DEPTH = 5;
constexpr int RFP_MARGIN = 120;

// Futility pruning: how close to the leaves, & how far below alpha the eval must be: 200 at depth 1, 325 at depth 2...
constexpr int FUTILITY_MAX_DEPTH = 6;
constexpr int FUTILITY_MARGIN_BASE = 75;
constexpr int FUTILITY_MARGIN_PER_DEPTH = 125;

// Late move pruning: how many moves are tried near the leaves before the quiet ones left are skipped, on top of
// depth squared: 4 at depth 1, 7 at depth 2, then 12, 19, 28, 39
constexpr int LMP_BASE = 3;

// Aspiration windows: from this depth on, the root is searched within this much of the last depth's score
constexpr int ASPIRATION_MIN_DEPTH = 4;
constexpr int ASPIRATION_WINDOW = 50;

// History scores stay within +-HISTORY_MAX, well below the killers
constexpr int HISTORY_MAX = 16384;
constexpr int HISTORY_BONUS_MAX = 1600;

/// How much a quiet move's history score moves by: more when there was more depth left to search
static int historyBonus(int depth) {
    return std::min(16 * depth * depth, HISTORY_BONUS_MAX);
}

/// Nudge a quiet move's history score up (it worked) or down (it was tried first & didn't)
static void updateHistory(Board& board, int move, int bonus) {
    int& entry = board.searchHistory[board.pieces[GET_FROM(move)]][GET_TO(move)];
    entry += bonus - entry * abs(bonus) / HISTORY_MAX; // the nearer the limit, the less it moves. So new results outweigh old ones
}

/// Negamax alpha-beta search...
static int alphaBeta(int alpha, int beta, int depth, Board& board, SearchInfo& info, HashTable& table, int nullMoveAllowed) {
    ASSERT(checkBoard(board))

    // Out of depth? Then it's over to quiescence. But not while in check: quiescence would 'stand pat' on a position
    // that can't stand still (or is mate!), so the check extension below gets in first & the replies are searched here
    if (depth == 0 && !isInCheck(board)) {
        return quiescence(alpha, beta, board, info); // quiescence counts this node
    }
    // Check time remaining:
    if ((board.nodes & 2047) == 0) {
        checkUp(board, info);
    }
    board.nodes++;

    if ((isRepetition(board) || board.fiftyMove >= 100) && board.ply) { // not at the root, or we'd have no move to play
        return 0;
    }
    if (board.ply > MAX_DEPTH - 1) { // at max depth allowed!
        return evalPosition(board);
    }

    // increase depth if we are in check!
    int inCheck = isInCheck(board); // are we in check? (makeMove wants to know too, so it's remembered)
    if (inCheck == true) {
        depth++;
    }

    int score = -ALPHABETA_BOUND;
    int pvMove = NO_MOVE;

    if (probeHashEntry(board, table, &pvMove, &score, alpha, beta, depth) == true) {
        // the root, answered by the table: its move came in the same read as its score. But only a score that
        // beats alpha comes with a best move: 'no better than alpha' just has whichever move did least badly
        if (!board.ply && score > alpha) {
            board.rootMove = pvMove;
        }
        return score;
    }

    // The two futility prunings trust the static eval near the leaves. Not in check, at the root or on the main line
    // (where beta - alpha > 1), so that is the only place the eval is worked out
    int nearLeaf = !inCheck && board.ply && beta - alpha == 1 && depth <= FUTILITY_MAX_DEPTH;
    int eval = nearLeaf ? evalPosition(board) : 0;

    // Reverse futility: a position already well above beta isn't going to fall below it. So don't look
    if (nearLeaf && depth <= RFP_MAX_DEPTH && abs(beta) < IS_MATE && eval - RFP_MARGIN * depth >= beta) {
        return beta;
    }
    // Futility: so far below alpha that no quiet move will make up the difference in the depth that's left.
    // Not with only pawns & a king (bigPieces counts the king), where one quiet move can decide the game
    int futile = nearLeaf && abs(alpha) < IS_MATE && board.bigPieces[board.side] > 1
                 && eval + FUTILITY_MARGIN_BASE + FUTILITY_MARGIN_PER_DEPTH * depth <= alpha;

    // Late move pruning: moves are tried best first, so near the leaves, once this many have been, a quiet move still
    // waiting is very unlikely to be the one. Wherever futility could apply, whatever the eval says
    int lateMoveLimit = nearLeaf && board.bigPieces[board.side] > 1 ? LMP_BASE + depth * depth : MAX_POSITION_MOVES;

    // if regular ab search was called AND not in check AND at least one move in AND do we have at least **1 big piece no including king** AND our depth is >= 4
    if (nullMoveAllowed && !inCheck && board.ply && (board.bigPieces[board.side] > 1) && depth >= 4) {
        // if so, we can make a null move
        makeNullMove(board);
        score = -alphaBeta(-beta, -beta+1, depth-4, board, info, table, false);
        undoNullMove(board);
        if (info.stopped.load(std::memory_order_relaxed)) {
            return 0;
        }
        if (score >= beta && abs(score) < IS_MATE) {
            board.nullCut++;
            return beta;
        }
    }

    MoveList moveList;
    generateAllMoves(board, moveList);
    int legal = 0;
    int oldAlpha = alpha;
    int bestMove = NO_MOVE;
    int bestScore = -ALPHABETA_BOUND;
    int quietsTried[MAX_POSITION_MOVES]; // the non-captures searched so far that didn't cut off
    int quietCount = 0;

    if (pvMove != NO_MOVE) { // if we have a principal variation move
        for (int moveNum = 0; moveNum < moveList.count; ++moveNum) {
            if (moveList.moves[moveNum].move == pvMove) { // if pv move is in our move list
                moveList.moves[moveNum].score = 2000000; // give it a big score in our list so it is searched first!
            }
        }
    }

    // Using principal variation search
    int foundPV = false;
    int skipQuiets = false; // set once late move pruning has given up on this node's quiet moves

    // for each move generated
    for (int moveNum = 0; moveNum < moveList.count; ++moveNum) {
        // order the moves. Once the quiet moves are being skipped there's no need: captures all come before them,
        // so the only moves left that aren't skipped are promotions without a capture
        if (!skipQuiets) {
            pickNextMove(moveNum, moveList);
        }
        int move = moveList.moves[moveNum].move;
        if (skipQuiets && !(move & MFLAG_PROMOTION)) {
            ASSERT(!(move & MFLAG_CAPTURING))
            continue;
        }
        // a late move reduction is for quiet moves (no capture, no promotion) that aren't killers at this ply
        int reducible = !(move & (MFLAG_CAPTURING | MFLAG_PROMOTION))
                        && move != board.searchKillers[0][board.ply] && move != board.searchKillers[1][board.ply];
        // at a futile node, once one move has been searched, the quiet ones aren't worth making, let alone searching
        if (futile && legal > 0 && !(move & (MFLAG_CAPTURING | MFLAG_PROMOTION))) {
            continue;
        }
        // Late move pruning: enough moves have been tried here, so this quiet move & the ones after it are skipped.
        // Killers are still tried (they come before the other quiet moves). Not while every move so far gets mated
        if (reducible && legal >= lateMoveLimit && bestScore > -IS_MATE) {
            skipQuiets = true;
            continue;
        }
        // skip if it is not legal
        if (!makeMove(board, move)) {
            continue;
        }
        legal++;
        // Late move reduction: moves are ordered best first, so a late quiet move is rarely any good.
        // Look at it a ply or more shallower, just to ask 'does it beat alpha?'. Not when in check, or giving check
        score = alpha + 1; // anything not reduced goes straight to the full search below
        if (reducible && legal > LMR_FULL_MOVES && depth >= LMR_MIN_DEPTH && !inCheck && !isInCheck(board)) {
            // by more for the later moves & at higher depths, but always leaving a ply to search
            int reduction = lmrReductions[std::min(depth, MAX_DEPTH - 1)][std::min(legal, LMR_MAX_MOVES - 1)];
            reduction = std::min(reduction, depth - 2);
            score = -alphaBeta(-alpha - 1, -alpha, depth-1-reduction, board, info, table, true);
        }
        // if it does (or it wasn't reduced), search it at full depth
        if (score > alpha) {
            // negamax, so flip it all. Also using principal variation search!
            // TODO: PVS may be detrimental...
            if (foundPV == true) {
                score = -alphaBeta(-alpha - 1, -alpha, depth-1, board, info, table, true);
                if (score > alpha && score < beta) {
                    score = -alphaBeta(-beta, -alpha, depth-1, board, info, table, true);
                }
            } else {
                score = -alphaBeta(-beta, -alpha, depth-1, board, info, table, true);
            }
        }
        undoMove(board);

        if (info.stopped.load(std::memory_order_relaxed)) { // if we have been told to stop, stop, but AFTER TAKEMOVE!!
            return 0;
        }

         if (score > bestScore) {
            bestScore = score;
            bestMove = moveList.moves[moveNum].move;
            if (score > alpha) {     // if score is greater than alpha...
                if (score >= beta) { // if we have beta cutoff, return beta...
                    if (!board.ply) {
                        board.rootMove = bestMove;
                    }
                    // A killer move is a move that has caused a beta cutoff but isn't a capture move:
                    if (!(moveList.moves[moveNum].move & MFLAG_CAPTURING)) {
                        // if criteria for killer move is met, shuffle killer moves down by 1...
                        board.searchKillers[1][board.ply] = board.searchKillers[0][board.ply];
                        // ...and add current move
                        board.searchKillers[0][board.ply] = moveList.moves[moveNum].move;
                        // ...and its history score goes up, while the quiet moves tried before it go down
                        int bonus = historyBonus(depth);
                        updateHistory(board, bestMove, bonus);
                        for (int i = 0; i < quietCount; ++i) {
                            updateHistory(board, quietsTried[i], -bonus);
                        }
                    }
                    storeHashEntry(board, table,  bestMove, beta, HF_BETA, depth);
                    return beta;
                }
                foundPV = true;
                alpha = score;       // otherwise, update score and bestMove
                bestMove = moveList.moves[moveNum].move;
                if (!board.ply) {
                    board.rootMove = bestMove;
                }
                // Update search history
                if (!(moveList.moves[moveNum].move & MFLAG_CAPTURING)) {
                    // by more when there's more depth left, to prioritise moves nearer the root of the tree
                    updateHistory(board, bestMove, historyBonus(depth));
                }

            }
        }
        if (!(move & MFLAG_CAPTURING)) { // no cutoff from this one. Remember it, in case a later move gets one
            quietsTried[quietCount++] = move;
        }
    }
    if (legal == 0) { // if we don't have any legal moves...
        // ...and the king square of our side is attacked by the opposite side, we are checkmated
        if (inCheck) {
            return -ALPHABETA_BOUND + board.ply; // return mate score PLUS PLYS TO GO UNTIL MATE
        } else {
            return 0;
        }
    }
    if (alpha != oldAlpha) { // if these don't match, then we must have updated alpha
        storeHashEntry(board, table,  bestMove, bestScore, HF_EXACT, depth); // so store it in the pv table
    } else {
        storeHashEntry(board, table,  bestMove, alpha, HF_ALPHA, depth);
    }
    return alpha;
}

/// The first legal move, so we always have something to play. NO_MOVE if there are none
static int firstLegalMove(Board& board) {
    MoveList moveList;
    generateAllMoves(board, moveList);
    for (int moveNum = 0; moveNum < moveList.count; ++moveNum) {
        if (makeMove(board, moveList.moves[moveNum].move)) {
            undoMove(board);
            return moveList.moves[moveNum].move;
        }
    }
    return NO_MOVE;
}

/// Score as uci wants it: 'cp x', or 'mate y' with y in moves (negative if we're getting mated)
static std::string scoreToUci(const int score) {
    if (score > IS_MATE) return "mate " + std::to_string((ALPHABETA_BOUND - score + 1) / 2);
    if (score < -IS_MATE) return "mate " + std::to_string(-(ALPHABETA_BOUND + score) / 2);
    return "cp " + std::to_string(score);
}

/// Iterative deepening on this thread's own board: depth 1, then 2 & so on, until told to stop.
/// The main thread prints each depth, remembers the move to play & watches the soft time limit.
/// A helper does none of that: what it finds reaches the others through the table
static void deepen(Board& board, SearchInfo& info, HashTable& table, int& bestMove, int& ponderMove) {
    const bool mainThread = board.threadId == 0;
    int lastScore = 0;

    for (int currentDepth = 1; currentDepth <= info.depth; ++currentDepth) {
        // Aspiration window: expect a score close to the last depth's, as a narrow window cuts off more.
        // Not for the first few depths, where scores jump about, nor around a mate score
        int alpha = -ALPHABETA_BOUND;
        int beta = ALPHABETA_BOUND;
        int step = ASPIRATION_WINDOW;
        if (currentDepth >= ASPIRATION_MIN_DEPTH && abs(lastScore) < IS_MATE) {
            alpha = lastScore - step;
            beta = lastScore + step;
        }
        int bestScore;
        int pvArray[MAX_DEPTH];
        int pvMoves = 0;
        // if the score lands outside the window, search again with that side moved out, twice as far each time
        while (true) {
            bestScore = alphaBeta(alpha, beta, currentDepth, board, info, table, true);
            if (info.stopped) { // Because we may have stopped partway through alphaBeta, we break here so the previous results are returned
                break;
            }
            if (bestScore <= alpha && alpha > -ALPHABETA_BOUND) { // worse than expected. All we know is 'at most alpha'
                alpha = std::max(alpha - step, -ALPHABETA_BOUND);
            } else {
                // inside the window, or better than expected: either way the search has a move to play.
                // The rest of the line is the table's
                if (mainThread) {
                    pvMoves = getPVLine(currentDepth, board, table, board.rootMove, pvArray);
                    if (pvMoves > 0) {
                        bestMove = pvArray[0];
                        ponderMove = pvArray[1]; // NO_MOVE if the pv is just the one move
                    }
                }
                if (bestScore < beta || beta == ALPHABETA_BOUND) { // inside the window, so this depth is done
                    break;
                }
                beta = std::min(beta + step, ALPHABETA_BOUND);
            }
            step *= 2;
        }
        if (info.stopped) {
            break;
        }
        lastScore = bestScore;
        if (!mainThread) {
            continue;
        }

        std::string pv;
        for (int pvNum = 0; pvNum < pvMoves; ++pvNum) {
            pv += ' ';
            pv += printMove(pvArray[pvNum]);
        }
        TimeMS time = getTimeMS() - info.startTime;
        long long nodes = board.nodes + info.helperNodes.load(std::memory_order_relaxed);
        sendLine("info depth %d seldepth %d score %s nodes %lld nps %lld hashfull %d time %lld pv%s",
                 currentDepth, board.selDepth, scoreToUci(bestScore).c_str(), nodes,
                 (long long)(nodes * 1000 / (time + 1)), hashFull(table), (long long)time, pv.c_str());
        // Most of the time gone? The next depth would take longer than all of this one, & a depth cut short is thrown away
        if (info.timeSet && getTimeMS() > info.softStopTime) {
            if (debugMode) {
                sendLine("info string stopped at the soft limit, after depth %d", currentDepth);
            }
            break;
        }
    }
}

/// Search the position & send 'bestmove'. This is the main search thread. With info.threads above 1 it starts that many
/// less one helpers, each searching the same position on a board of its own, & has stopped them all before it answers
void searchPosition(Board& board, SearchInfo& info, HashTable& table) {
    clearForSearch(board, info, table);
    int bestMove = NO_MOVE;
    int ponderMove = NO_MOVE;

    // each helper gets a copy of the board (on the heap, it's big), & so killers, history scores & counts of its own
    std::vector<std::unique_ptr<Board>> helperBoards;
    std::vector<std::thread> helpers;
    for (int id = 1; id < info.threads; ++id) {
        helperBoards.push_back(std::make_unique<Board>(board));
        helperBoards.back()->threadId = id;
        helpers.emplace_back([&info, &table, helper = helperBoards.back().get()] {
            int unusedMove = NO_MOVE, unusedPonder = NO_MOVE;
            deepen(*helper, info, table, unusedMove, unusedPonder);
        });
    }

    deepen(board, info, table, bestMove, ponderMove);

    if (debugMode && info.stopped && info.timeSet && getTimeMS() > info.stopTime) {
        sendLine("info string stopped at the hard limit");
    }
    // stopped before depth 1 was done? Play anything legal. Only now, as making & undoing a move
    // can reorder the piece lists, and doing that before the search would change it
    if (bestMove == NO_MOVE) {
        bestMove = firstLegalMove(board);
    }

    // a pv of one move leaves nothing to ponder on, so ask the table for a reply
    if (bestMove != NO_MOVE && ponderMove == NO_MOVE) {
        ponderMove = probePonderMove(board, bestMove, table);
    }

    // 'go infinite' & pondering mustn't send bestmove until the gui says so. The helpers search on meanwhile
    while ((info.infinite || info.ponder) && !info.stopped) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // the helpers stop when told to, like any search, & must be gone before the gui hears the move:
    // its next command may be for the table
    info.nodes = board.nodes;
    info.selDepth = board.selDepth;
    if (!helpers.empty()) {
        info.stopped = true;
        for (size_t i = 0; i < helpers.size(); ++i) {
            helpers[i].join();
            info.nodes += helperBoards[i]->nodes;
        }
    }
    std::string best = printMove(bestMove); // copy, as printMove reuses its buffer
    if (ponderMove != NO_MOVE) {
        sendLine("bestmove %s ponder %s", best.c_str(), printMove(ponderMove));
    } else {
        sendLine("bestmove %s", best.c_str());
    }
}
