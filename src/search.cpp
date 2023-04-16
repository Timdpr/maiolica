#include "defs.h"
#include <cstring>

/// Check if time is up / interrupt from gui
static void checkUp(SearchInfo *info) {
    if (info->timeSet == true && getTimeMS() > info->stopTime) {
        info->stopped = true;
    }
}

/// Swap the move at 'moveNum' index with the 'best' move in the moveList
static void pickNextMove(int moveNum, MoveList *moveList) {
    int bestScore = 0;
    int bestMoveNum = moveNum;
    for (int i = moveNum; i < moveList->count; ++i) { // find move with highest score
        if (moveList->moves[i].score > bestScore) {
            bestScore = moveList->moves[i].score;
            bestMoveNum = i;
        }
    }
    ASSERT(moveNum >= 0 && moveNum < moveList->count)
    ASSERT(bestMoveNum >= 0 && bestMoveNum < moveList->count)
    ASSERT(bestMoveNum >= moveNum)
    // and swap it with move at 'moveNum'
    Move tempMove = moveList->moves[moveNum];
    moveList->moves[moveNum] = moveList->moves[bestMoveNum];
    moveList->moves[bestMoveNum] = tempMove;
}

static int isRepetition(const Board& board) {
    // start at current ply minus how long since the fifty move rule was reset, and look at hash keys
    for (int i = board.historyPly - board.fiftyMove; i < board.historyPly-1; ++i) { // todo: quicker way to do this in yt comments?
        ASSERT(i >= 0 && i < MAX_GAME_MOVES)
        // if current hash key matches, then this must be a repetition!
        if (board.positionKey == board.history[i].positionKey) {
            return true;
        }
    }
    return false;
}

/// Clear relevant arrays before starting to search and set up search info
static void clearForSearch(Board& board, SearchInfo *info, HashTable *table) {
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
    table->hit = 0;
    table->cut = 0;
    table->currentAge++;

    board.ply = 0;

    info->stopped = 0;
    info->nodes = 0;
    info->failHigh = 0;
    info->failHighFirst = 0;
}

static int quiescence(int alpha, int beta, Board& board, SearchInfo *info) {
    ASSERT(checkBoard(board))
    ASSERT(beta > alpha)
    // Check time remaining:
    if ((info->nodes & 2047) == 0) {
        checkUp(info);
    }
    info->nodes++;

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

    MoveList moveList[1];
    generateAllCaptureMoves(board, moveList); // note - only capture moves!
    int legal = 0;

    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        // order the moves
        pickNextMove(moveNum, moveList);
        // skip if it is not legal
        if (!makeMove(board, moveList->moves[moveNum].move)) {
            continue;
        }
        legal++;
        // negamax, so flip it all...
        score = -quiescence(-beta, -alpha, board, info);
        undoMove(board);

        if (info->stopped == true) { // if we have been told to stop, stop, but AFTER TAKEMOVE!!
            return 0;
        }

        if (score > alpha) {     // if score is greater than alpha...
            if (score >= beta) { // if we have beta cutoff, return beta...
                if (legal == 1) {
                    info->failHighFirst++; // increment 'we searched the best move first' counter
                }
                info->failHigh++;
                return beta;
            }
            alpha = score;       // otherwise, update score and bestMove
//            bestMove = moveList->moves[moveNum].move;
        }
    }
    // todo: should we store the best move in the hash table (if alpha != the   alpha given to the function) here?
    return alpha;
}

/// Negamax alpha-beta search...
static int alphaBeta(int alpha, int beta, int depth, Board& board, SearchInfo *info, HashTable *table, int nullMoveAllowed) {
    ASSERT(checkBoard(board))

    if (depth == 0) {
        info->nodes++;
        return quiescence(alpha, beta, board, info);
    }
    // Check time remaining:
    if ((info->nodes & 2047) == 0) {
        checkUp(info);
    }
    info->nodes++;

    if (isRepetition(board) || board.fiftyMove >= 100) {
        return 0;
    }
    if (board.ply > MAX_DEPTH - 1) { // at max depth allowed!
        return evalPosition(board);
    }

    // increase depth if we are in check!
    int inCheck = isSquareAttacked(board.kingSq[board.side], board.side^1, board); // are we in check?
    if (inCheck == true) {
        depth++;
    }

    int score = -ALPHABETA_BOUND;
    int pvMove = NO_MOVE;

    if (probeHashEntry(board, table, &pvMove, &score, alpha, beta, depth) == true) {
        table->cut++;
        return score;
    }

    // if regular ab search was called AND not in check AND at least one move in AND do we have at least **1 big piece no including king** AND our depth is >= 4
    if (nullMoveAllowed && !inCheck && board.ply && (board.bigPieces[board.side] > 1) && depth >= 4) {
        // if so, we can make a null move
        makeNullMove(board);
        score = -alphaBeta(-beta, -beta+1, depth-4, board, info, table, false);
        undoNullMove(board);
        if (info->stopped == true) {
            return 0;
        }
        if (score >= beta && abs(score) < IS_MATE) {
            info->nullCut++;
            return beta;
        }
    }

    MoveList moveList[1];
    generateAllMoves(board, moveList);
    int legal = 0;
    int oldAlpha = alpha;
    int bestMove = NO_MOVE;
    int bestScore = -ALPHABETA_BOUND;

    if (pvMove != NO_MOVE) { // if we have a principal variation move
        for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
            if (moveList->moves[moveNum].move == pvMove) { // if pv move is in our move list
                moveList->moves[moveNum].score = 2000000; // give it a big score in our list so it is searched first!
            }
        }
    }

    // Using principal variation search
    int foundPV = false;

    // for each move generated
    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        // order the moves
        pickNextMove(moveNum, moveList);
        // skip if it is not legal
        if (!makeMove(board, moveList->moves[moveNum].move)) {
            continue;
        }
        legal++;
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
        undoMove(board);

        if (info->stopped == true) { // if we have been told to stop, stop, but AFTER TAKEMOVE!!
            return 0;
        }

         if (score > bestScore) {
            bestScore = score;
            bestMove = moveList->moves[moveNum].move;
            if (score > alpha) {     // if score is greater than alpha...
                if (score >= beta) { // if we have beta cutoff, return beta...
                    if (legal == 1) {
                        info->failHighFirst++; // increment 'we searched the best move first' counter
                    }
                    info->failHigh++;
                    // A killer move is a move that has caused a beta cutoff but isn't a capture move:
                    if (!(moveList->moves[moveNum].move & MFLAG_CAPTURING)) {
                        // if criteria for killer move is met, shuffle killer moves down by 1...
                        board.searchKillers[1][board.ply] = board.searchKillers[0][board.ply];
                        // ...and add current move
                        board.searchKillers[0][board.ply] = moveList->moves[moveNum].move;
                    }
                    storeHashEntry(board, table,  bestMove, beta, HF_BETA, depth);
                    return beta;
                }
                foundPV = true;
                alpha = score;       // otherwise, update score and bestMove
                bestMove = moveList->moves[moveNum].move;
                // Update search history
                if (!(moveList->moves[moveNum].move & MFLAG_CAPTURING)) {
                    // increment by depth to prioritise moves nearer the root of the tree
                    board.searchHistory[board.pieces[GET_FROM(bestMove)]][GET_TO(bestMove)] += depth;
                }

            }
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

int searchPositionThread(SearchThreadData *searchData) {
//    auto *searchData = (SearchThreadData *)data;
    // Create new empty board, deep copy original position into it, then search

    // C way:
    auto *board = (Board *)malloc(sizeof(Board));
    memcpy(board, searchData->originalPosition, sizeof(Board));
    searchPosition(*board, searchData->info, searchData->ttable);
    free(board);

    // C++ way:
//    auto board = new Board{};
//    std::memcpy(board, searchData->originalPosition, sizeof(Board));
//    searchPosition(*board, searchData->info, searchData->ttable);
//    delete board;

    return 0;
}

void searchPosition(Board& board, SearchInfo *info, HashTable *table) {
    clearForSearch(board, info, table);
    int bestMove = board.pvArray[0];

    for (int currentDepth = 1; currentDepth <= info->depth; ++currentDepth) {
        int bestScore = alphaBeta(-ALPHABETA_BOUND, ALPHABETA_BOUND, currentDepth, board, info, table, true);
        if (info->stopped == true) { // Because we may have stopped partway through alphaBeta, we break here so the previous results are returned
            break;
        }
        int pvMoves = getPVLine(currentDepth, board, table);
        bestMove = board.pvArray[0];

        printf("info score cp %d depth %d nodes %ld time %lld pv",
               bestScore, currentDepth, info->nodes, getTimeMS() - info->startTime);

        for (int pvNum = 0; pvNum < pvMoves; ++pvNum) {
            printf(" %s", printMove(board.pvArray[pvNum]));
        }
        printf("\n");
    }
     printf("bestmove %s\n", printMove(bestMove));
}
