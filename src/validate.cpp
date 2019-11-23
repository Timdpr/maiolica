#include "defs.h"

/// Not using bools here, as compiler adds 'not 0 or 1' checking, and ints should be determined to be 8 bit here anyway

int squareOnBoard(const int sq) {
    return filesBoard[sq] == OFFBOARD ? 0 : 1;
}

int sideValid(const int side) {
    return (side == WHITE || side == BLACK) ? 1 : 0;
}

int fileOrRankValid(const int fileOrRank) {
    return (fileOrRank >= 0 && fileOrRank <= 7) ? 1 : 0;
}

int pieceValid(const int piece) {
    return (piece >= wP && piece <= bK) ? 1 : 0;
}

int pieceValidOrEmpty(const int piece) {
    return (piece >= EMPTY && piece <= bK) ? 1 : 0;
}