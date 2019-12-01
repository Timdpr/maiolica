#include "defs.h"

const int pawnIsolated = -10; // isolated pawn penalty
const int pawnPassed[8] = { 0, 5, 10, 20, 35, 60, 100, 200 }; // passed pawn value by rank
const int rookOpenFile = 5;

const int pawnTable[64] = {
        0	,	0	,	0	,	0	,	0	,	0	,	0	,	0	,
        10	,	10	,	0	,	-10	,	-10	,	0	,	10	,	10	,
        5	,	0	,	0	,	5	,	5	,	0	,	0	,	5	,
        0	,	0	,	10	,	20	,	20	,	10	,	0	,	0	,
        5	,	5	,	5	,	10	,	10	,	5	,	5	,	5	,
        10	,	10	,	10	,	20	,	20	,	10	,	10	,	10	,
        20	,	20	,	20	,	30	,	30	,	20	,	20	,	20	,
        0	,	0	,	0	,	0	,	0	,	0	,	0	,	0
};

const int knightTable[64] = {
        0	,	-10	,	0	,	0	,	0	,	0	,	-10	,	0	,
        0	,	0	,	0	,	5	,	5	,	0	,	0	,	0	,
        0	,	0	,	10	,	10	,	10	,	10	,	0	,	0	,
        0	,	0	,	10	,	20	,	20	,	10	,	0	,	0	,
        5	,	10	,	15	,	20	,	20	,	15	,	10	,	5	,
        5	,	10	,	10	,	20	,	20	,	10	,	10	,	5	,
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0	,
        0	,	0	,	0	,	0	,	0	,	0	,	0	,	0
};

const int bishopTable[64] = {
        0	,	0	,	-10	,	0	,	0	,	-10	,	0	,	0	,
        0	,	0	,	0	,	10	,	10	,	0	,	0	,	0	,
        0	,	0	,	10	,	15	,	15	,	10	,	0	,	0	,
        0	,	10	,	15	,	20	,	20	,	15	,	10	,	0	,
        0	,	10	,	15	,	20	,	20	,	15	,	10	,	0	,
        0	,	0	,	10	,	15	,	15	,	10	,	0	,	0	,
        0	,	0	,	0	,	10	,	10	,	0	,	0	,	0	,
        0	,	0	,	0	,	0	,	0	,	0	,	0	,	0
};

const int rookTable[64] = {
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0	,
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0	,
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0	,
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0	,
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0	,
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0	,
        25	,	25	,	25	,	25	,	25	,	25	,	25	,	25	,
        0	,	0	,	5	,	10	,	10	,	5	,	0	,	0
};

/// Evaluate a board position! @return a score (in 100ths of a pawn)
int evalPosition(const Board *board) {
    int score = board->material[WHITE] - board->material[BLACK]; // compare material score (remember white is +vs, black -ve!)

    // Get scores from tables for each piece type:
    int piece = wP;
    for (int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) { // for as many pieces of this type that are on the board...
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score += pawnTable[INDEX_120_TO_64(square)];
        // Check for isolated pawn: AND the pawn mask for the square with the pawn bitboard
        if ((isolatedPawnMask[INDEX_120_TO_64(square)] & board->pawns[WHITE]) == 0) {
//            printf("wP Isolated: %s\n", printSquare(square));
            score += pawnIsolated;
        }
        // Check for passed pawn
        if ((whitePassedMask[INDEX_120_TO_64(square)] & board->pawns[BLACK]) == 0) {
//            printf("wP Passed: %s\n", printSquare(square));
            score += pawnPassed[ranksBoard[square]];
        }
    }

    piece = bP;
    for (int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score -= pawnTable[MIRROR_64(INDEX_120_TO_64(square))];
        // Check for isolated pawn: AND the pawn mask for the square with the pawn bitboard
        if ((isolatedPawnMask[INDEX_120_TO_64(square)] & board->pawns[BLACK]) == 0) {
//            printf("bP Isolated: %s\n", printSquare(square));
            score -= pawnIsolated;
        }
        // Check for passed pawn
        if ((blackPassedMask[INDEX_120_TO_64(square)] & board->pawns[WHITE]) == 0) {
//            printf("bP Passed: %s\n", printSquare(square));
            score -= pawnPassed[7 - ranksBoard[square]]; // note '7 - the rank' because black pawns go down the board!
        }
    }

    piece = wN;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score += knightTable[INDEX_120_TO_64(square)];
    }

    piece = bN;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score -= knightTable[MIRROR_64(INDEX_120_TO_64(square))];
    }

    piece = wB;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score += bishopTable[INDEX_120_TO_64(square)];
    }

    piece = bB;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score -= bishopTable[MIRROR_64(INDEX_120_TO_64(square))];
    }

    piece = wR;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score += rookTable[INDEX_120_TO_64(square)];
    }

    piece = bR;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score -= rookTable[MIRROR_64(INDEX_120_TO_64(square))];
    }

    return board->side == WHITE ? score : -score; // flip score if side is black
}