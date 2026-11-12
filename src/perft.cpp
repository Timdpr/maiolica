#include "defs.h"
#include <cstdio>

long long leafNodes;

void perft(int depth, Board& board) {
    ASSERT(checkBoard(board))

    if (depth == 0) {
        leafNodes++;
        return;
    }

    MoveList moveList;
    generateAllMoves(board, moveList);

    for (int moveNum = 0; moveNum < moveList.count; ++moveNum) {
        if (!makeMove(board, moveList.moves[moveNum].move)) {
            continue;
        }
        perft(depth-1, board);
        undoMove(board);
    }
}

void perftTest(int depth, Board& board) {
    ASSERT(checkBoard(board))


    leafNodes = 0;
    TimeMS start = getTimeMS();

    MoveList moveList;
    generateAllMoves(board, moveList);

    for (int moveNum = 0; moveNum < moveList.count; ++moveNum) {
        int move = moveList.moves[moveNum].move;

        if (!makeMove(board, move)) {
            continue;
        }

        long long cumNodes = leafNodes;
        perft(depth - 1, board);

        undoMove(board);
        long long oldNodes = leafNodes - cumNodes;
        printf("%s: %lld\n", printMove(move), oldNodes);
    }

    printf("\nNodes searched: %lld (%lld ms)\n", leafNodes, (long long)(getTimeMS() - start));
}