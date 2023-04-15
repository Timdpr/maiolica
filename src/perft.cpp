#include "defs.h"
#include <cstdio>

long leafNodes;

void perft(int depth, Board& board) {
    ASSERT(checkBoard(board))

    if (depth == 0) {
        leafNodes++;
        return;
    }

    MoveList moveList[1];
    generateAllMoves(board, moveList);

    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        if (!makeMove(board, moveList->moves[moveNum].move)) {
            continue;
        }
        perft(depth-1, board);
        undoMove(board);
    }
}

void perftTest(int depth, Board& board) {
    ASSERT(checkBoard(board))

    printBoard(board);
    printf("\nStarting test to depth: %d\n", depth);

    leafNodes = 0;
    TimeMS start = getTimeMS();

    MoveList moveList[1];
    generateAllMoves(board, moveList);

    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        int move = moveList->moves[moveNum].move;

        if (!makeMove(board, move)) {
            continue;
        }

        long cumNodes = leafNodes;
        perft(depth - 1, board);

        undoMove(board);
        long oldNodes = leafNodes - cumNodes;
        printf("Move %d : %s : %ld\n", moveNum+1, printMove(move), oldNodes);
    }

    printf("Test complete: %ld leaf nodes visited in %lld ms\n", leafNodes, getTimeMS() - start);
}