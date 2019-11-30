#include "defs.h"

#define INFINITE 30000
#define MATE 29000

/// Check if time is up / interrupt from gui
static void checkUp(SearchInfo *info) {
    if (info->timeSet == TRUE && getTimeMS() > info->stopTime) {
        info->stopped = TRUE;
    }
    ReadInput(info);
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
    } // and swap it with move at 'moveNum'
    Move tempMove = moveList->moves[moveNum];
    moveList->moves[moveNum] = moveList->moves[bestMoveNum];
    moveList->moves[bestMoveNum] = tempMove;
}

static int isRepetition(const Board *board) {
    // start at current ply minus how long since the fifty move rule was reset, and look at hash keys
    for (int i = board->historyPly - board->fiftyMove; i < board->historyPly-1; ++i) { // todo: quicker way to do this in yt comments?
        ASSERT(i >= 0 && i < MAX_GAME_MOVES)
        // if current hash key matches, then this must be a repetition!
        if (board->positionKey == board->history[i].positionKey) {
            return TRUE;
        }
    }
    return FALSE;
}

/// Clear relevant arrays before starting to search and set up search info
static void clearForSearch(Board *board, SearchInfo *info) {
    for (int i = 0; i < 13; ++i) {
        for (int j = 0; j < BRD_SQ_NUM; ++j) {
            board->searchHistory[i][j] = 0;
        }
    }
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < MAX_DEPTH; ++j) {
            board->searchKillers[i][j] = 0;
        }
    }

    clearPVTable(board->pvTable);
    board->ply = 0;

    info->stopped = 0;
    info->nodes = 0;
    info->failHigh = 0;
    info->failHighFirst = 0;
}

static int quiescence(int alpha, int beta, Board *board, SearchInfo *info) {
    ASSERT(checkBoard(board))
    // Check time remaining:
    if ((info->nodes & 2047) == 0) {
        checkUp(info);
    }
    info->nodes++;

    if ((isRepetition(board) || board->fiftyMove >= 100) && board->ply) { // no repetition or 50 move AND we have at least one move
        return 0;
    }
    if (board->ply > MAX_DEPTH-1) { // at max depth allowed!
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
    int oldAlpha = alpha;
    int bestMove = NO_MOVE;

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
        takeMove(board);

        if (info->stopped == TRUE) { // if we have been told to stop, stop, but AFTER TAKEMOVE!!
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
            bestMove = moveList->moves[moveNum].move;
        }
    }
    if (alpha != oldAlpha) {
        storePVMove(board, bestMove);
    }
    return alpha;
}

/// Negamax alpha-beta search...
static int alphaBeta(int alpha, int beta, int depth, Board *board, SearchInfo *info, int nullAllowed) {
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

    if (isRepetition(board) || board->fiftyMove >= 100) {
        return 0;
    }
    if (board->ply > MAX_DEPTH-1) { // at max depth allowed!
        return evalPosition(board);
    }

    MoveList moveList[1];
    generateAllMoves(board, moveList);
    int legal = 0;
    int oldAlpha = alpha;
    int bestMove = NO_MOVE;

    int pvMove = probePVTable(board);
    if (pvMove != NO_MOVE) { // if we have a principal variation move
        for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
            if (moveList->moves[moveNum].move == pvMove) { // if pv move is in our move list
                moveList->moves[moveNum].score = 2000000; // give it a big score in our list so it is searched first!
            }
        }
    }

    // for each move generated
    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        // order the moves
        pickNextMove(moveNum, moveList);
        // skip if it is not legal
        if (!makeMove(board, moveList->moves[moveNum].move)) {
            continue;
        }
        legal++;
        // negamax, so flip it all...
        int score = -alphaBeta(-beta, -alpha, depth-1, board, info, TRUE);
        takeMove(board);

        if (info->stopped == TRUE) { // if we have been told to stop, stop, but AFTER TAKEMOVE!!
            return 0;
        }

        if (score > alpha) {     // if score is greater than alpha...
            if (score >= beta) { // if we have beta cutoff, return beta...
                if (legal == 1) {
                    info->failHighFirst++; // increment 'we searched the best move first' counter
                }
                info->failHigh++;
                // A killer move is a move that has caused a beta cutoff but isn't a capture move:
                if (!(moveList->moves[moveNum].move & MFLAG_CAPTURING)) {
                    // if criteria for killer move is met, shuffle killer moves down by 1...
                    board->searchKillers[1][board->ply] = board->searchKillers[0][board->ply];
                    // ...and add current move
                    board->searchKillers[0][board->ply] = moveList->moves[moveNum].move;
                }
                return beta;
            }
            alpha = score;       // otherwise, update score and bestMove
            bestMove = moveList->moves[moveNum].move;
            // Update search history
            if (!(moveList->moves[moveNum].move & MFLAG_CAPTURING)) {
                // increment by depth to prioritise moves nearer the root of the tree
                board->searchHistory[board->pieces[GET_FROM(bestMove)]][GET_TO(bestMove)] += depth;
            }

        }
    }
    if (legal == 0) { // if we don't have any legal moves...
        // ...and the king square of our side is attacked by the opposite side, we are checkmated
        if (isSquareAttacked(board->kingSq[board->side], board->side^1, board)) {
            return -MATE + board->ply; // return mate score PLUS PLYS TO GO UNTIL MATE
        } else {
            return 0;
        }
    }
    if (alpha != oldAlpha) { // if these don't match, then we must have updated alpha
        storePVMove(board, bestMove); // so store it in the pv table
    }
    return alpha;
}

void searchPosition(Board *board, SearchInfo *info) {
    clearForSearch(board, info);
    int bestMove = board->pvArray[0];

    for (int currentDepth = 1; currentDepth <= info->depth; ++currentDepth) {
        int bestScore = alphaBeta(-INFINITE, INFINITE, currentDepth, board, info, TRUE);
        if (info->stopped == TRUE) {
            // Because we may have stopped partway through alphaBeta, we break here so the previous results are returned
            break;
        }
        int pvMoves = getPVLine(currentDepth, board);
        bestMove = board->pvArray[0];
        printf("info score cp %d depth %d nodes %ld time %lld ",
               bestScore, currentDepth, info->nodes, getTimeMS() - info->startTime);
        printf("pv");
        for (int pvNum = 0; pvNum < pvMoves; ++pvNum) {
            printf(" %s", printMove(board->pvArray[pvNum]));
        }
        printf("\n");
//        if (info->failHigh > 0) {
//            printf("Ordering: %.2f\n", (info->failHighFirst / info->failHigh));
//        } else {
//            printf("Ordering: failHigh was 0\n");
//        }
    }

    // info score cp 13  depth 1 nodes 13 time 15 pv f1b5
    printf("bestmove %s\n", printMove(bestMove));
}