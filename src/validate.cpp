#include "defs.h"

bool squareOnBoard(const int sq) {
    return !(filesBoard[sq] == OFFBOARD);
}

bool sideValid(const int side) {
    return side == WHITE || side == BLACK;
}

bool fileOrRankValid(const int fileOrRank) {
    return fileOrRank >= 0 && fileOrRank <= 7;
}

bool pieceValid(const int piece) {
    return piece >= wP && piece <= bK;
}

bool pieceValidOrEmpty(const int piece) {
    return piece >= EMPTY && piece <= bK;
}