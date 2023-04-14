#include "defs.h"
#include <cstdio>

/// Returns char array of the algebraic representation of the square
char *printSquare(const int square) {

    static char squareString[3];

    int file = filesBoard[square];
    int rank = ranksBoard[square];

    std::snprintf(squareString, sizeof(squareString), "%c%c", ('a' + file), ('1' + rank));

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
        snprintf(moveString, sizeof(moveString), "%c%c%c%c%c", ('a' + fileFrom), ('1' + rankFrom), ('a' + fileTo), ('1' + rankTo), promChar);
    } else {
        snprintf(moveString, sizeof(moveString), "%c%c%c%c", ('a' + fileFrom), ('1' + rankFrom), ('a' + fileTo), ('1' + rankTo));
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

/// Takes algebraic notation for move, and returns move int
int parseMove(const char *ptrChar, Board *board) {
    // Make sure that rank/file is within correct bounds
    if (ptrChar[1] > '8' || ptrChar[1] < '1') return false;
    if (ptrChar[3] > '8' || ptrChar[3] < '1') return false;
    if (ptrChar[0] > 'h' || ptrChar[0] < 'a') return false;
    if (ptrChar[2] > 'h' || ptrChar[2] < 'a') return false;
    // Get from/to square
    int from = FILE_RANK_TO_SQUARE_INDEX(ptrChar[0] - 'a', ptrChar[1] - '1');
    int to = FILE_RANK_TO_SQUARE_INDEX(ptrChar[2] - 'a', ptrChar[3] - '1');

    ASSERT(squareOnBoard(from) && squareOnBoard(to))

    // Generate all moves for this board position, then try to find one that matches the input
    MoveList moveList[1];
    generateAllMoves(board, moveList);

    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) { // loop through moves to find matching move
        int move = moveList->moves[moveNum].move;
        if (GET_FROM(move) == from && GET_TO(move) == to) { // the from & to squares match, so this is the same move...
            int promotedPiece = GET_PROMOTED(move); // ...if the promoted piece matches too:
            if (promotedPiece != EMPTY) {
                if (IS_ROOK_OR_QUEEN(promotedPiece) && !IS_BISHOP_OR_QUEEN(promotedPiece) && ptrChar[4] == 'r') {
                    return move;
                } else if (!IS_ROOK_OR_QUEEN(promotedPiece) && IS_BISHOP_OR_QUEEN(promotedPiece) && ptrChar[4] == 'b') {
                    return move;
                } else if (IS_ROOK_OR_QUEEN(promotedPiece) && IS_BISHOP_OR_QUEEN(promotedPiece) && ptrChar[4] == 'q') {
                    return move;
                } else if (IS_KNIGHT(promotedPiece) && ptrChar[4] == 'n') {
                    return move;
                }
                continue;
            }
            return move;
        }
    }
    return NO_MOVE;
}