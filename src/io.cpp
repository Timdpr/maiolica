#include "defs.h"
#include <cstdio>

/// Returns char array of the algebraic representation of the square
char *printSquare(const int square) {

    static char squareString[3];

    int file = filesBoard[square];
    int rank = ranksBoard[square];

    std::sprintf(squareString, "%c%c", ('a' + file), ('1' + rank));

    return squareString;
}

/// Returns char array of the algebraic representation of the move
char *printMove(const int move) {
    static char moveString[6];

    // Get elements of the move
    int fileFrom = filesBoard[GET_FROM(move)];
    int rankFrom = ranksBoard[GET_FROM(move)];
    int fileTo = filesBoard[GET_TO(move)];
    int rankTo = ranksBoard[GET_TO(move)];
    int promoted = GET_PROMOTED(move);

    if (promoted) {
        char promChar = 'q'; // assume promoted piece will be a queen
        if (IS_KNIGHT(promoted)) { // check if it's a knight
            promChar = 'n';
        } else if (IS_ROOK_OR_QUEEN(promoted) && !IS_BISHOP_OR_QUEEN(promoted)) { // check if it's a rook
            promChar = 'r';
        } else if (!IS_ROOK_OR_QUEEN(promoted) && IS_BISHOP_OR_QUEEN(promoted)) { // check if it's a bishop
            promChar = 'b';
        }
        sprintf(moveString, "%c%c%c%c%c", ('a' + fileFrom), ('1' + rankFrom), ('a' + fileTo), ('1' + rankTo), promChar);
    } else {
        sprintf(moveString, "%c%c%c%c", ('a' + fileFrom), ('1' + rankFrom), ('a' + fileTo), ('1' + rankTo));
    }
    return moveString;
}

void printMoveList(const MoveList *moveList) {
    printf("MoveList: %d moves\n", moveList->count);

    for (int i = 0; i < moveList->count; ++i) {
        int move = moveList->moves[i].move;
        int score = moveList->moves[i].score;

        printf("Move: %d > %s (score: %d)\n", i+1, printMove(move), score);
    }
}