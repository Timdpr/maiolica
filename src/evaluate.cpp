#include "defs.h"

const int pawnIsolated = -10; // isolated pawn penalty
const int pawnPassed[8] = { 0, 5, 10, 20, 35, 60, 100, 200 }; // passed pawn value by rank
const int rookOpenFile = 10;
const int rookSemiOpenFile = 5;
const int queenOpenFile = 5;
const int queenSemiOpenFile = 3;
const int bishopPair = 30;

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

/// Heavy penalties for corners! Encourages king to be central
const int kingEndgame[64] = {
        -50	,	-10	,	0	,	0	,	0	,	0	,	-10	,	-50	,
        -10,	0	,	10	,	10	,	10	,	10	,	0	,	-10	,
        0	,	10	,	15	,	15	,	15	,	15	,	10	,	0	,
        0	,	10	,	15	,	20	,	20	,	15	,	10	,	0	,
        0	,	10	,	15	,	20	,	20	,	15	,	10	,	0	,
        0	,	10	,	15	,	15	,	15	,	15	,	10	,	0	,
        -10,	0	,	10	,	10	,	10	,	10	,	0	,	-10	,
        -50	,	-10	,	0	,	0	,	0	,	0	,	-10	,	-50
};

/// Encouraged to castle kingside, penalised for walking forwards
const int kingOpening[64] = {
        0	,	5	,	5	,	-10	,	-10	,	0	,	10	,	5	,
        -30	,	-30	,	-30	,	-30	,	-30	,	-30	,	-30	,	-30	,
        -50	,	-50	,	-50	,	-50	,	-50	,	-50	,	-50	,	-50	,
        -70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
        -70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
        -70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
        -70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
        -70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70
};

/// Material score threshold to determine when we have moved into the endgame
#define ENDGAME_MATERIAL (1 * pieceValues[wR] + 2 * pieceValues[wN] + 2 * pieceValues[wP])

/// based on sjeng
int materialDraw(const Board *board) {
    if (!board->pieceCounts[wR] && !board->pieceCounts[bR] && !board->pieceCounts[wQ] && !board->pieceCounts[bQ]) {
        if (!board->pieceCounts[bB] && !board->pieceCounts[wB]) {
            if (board->pieceCounts[wN] < 3 && board->pieceCounts[bN] < 3) {  return TRUE; }
        } else if (!board->pieceCounts[wN] && !board->pieceCounts[bN]) {
            if (abs(board->pieceCounts[wB] - board->pieceCounts[bB]) < 2) { return TRUE; }
        } else if ((board->pieceCounts[wN] < 3 && !board->pieceCounts[wB]) || (board->pieceCounts[wB] == 1 && !board->pieceCounts[wN])) {
            if ((board->pieceCounts[bN] < 3 && !board->pieceCounts[bB]) || (board->pieceCounts[bB] == 1 && !board->pieceCounts[bN]))  { return TRUE; }
        }
    } else if (!board->pieceCounts[wQ] && !board->pieceCounts[bQ]) {
        if (board->pieceCounts[wR] == 1 && board->pieceCounts[bR] == 1) {
            if ((board->pieceCounts[wN] + board->pieceCounts[wB]) < 2 && (board->pieceCounts[bN] + board->pieceCounts[bB]) < 2)	{ return TRUE; }
        } else if (board->pieceCounts[wR] == 1 && !board->pieceCounts[bR]) {
            if ((board->pieceCounts[wN] + board->pieceCounts[wB] == 0) && (((board->pieceCounts[bN] + board->pieceCounts[bB]) == 1) || ((board->pieceCounts[bN] + board->pieceCounts[bB]) == 2))) { return TRUE; }
        } else if (board->pieceCounts[bR] == 1 && !board->pieceCounts[wR]) {
            if ((board->pieceCounts[bN] + board->pieceCounts[bB] == 0) && (((board->pieceCounts[wN] + board->pieceCounts[wB]) == 1) || ((board->pieceCounts[wN] + board->pieceCounts[wB]) == 2))) { return TRUE; }
        }
    }
    return FALSE;
}

/// Evaluate a board position! @return a score (in 100ths of a pawn)
int evalPosition(const Board *board) {
    if (board->pieceCounts[wP] && !board->pieceCounts[bP] && materialDraw(board) == TRUE) {
        return 0;
    }

    int score = board->material[WHITE] - board->material[BLACK]; // compare material score (remember white is +vs, black -ve!)

    // Get scores from tables for each piece type:
    int piece = wP;
    for (int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) { // for as many pieces of this type that are on the board...
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score += pawnTable[INDEX_120_TO_64(square)];
        // Check for isolated pawn: AND the pawn mask for the square with the pawn bitboard
        if ((isolatedPawnMask[INDEX_120_TO_64(square)] & board->pawns[WHITE]) == 0) {
            score += pawnIsolated;
        }
        // Check for passed pawn
        if ((whitePassedMask[INDEX_120_TO_64(square)] & board->pawns[BLACK]) == 0) {
            score += pawnPassed[ranksBoard[square]];
        }
    }

    piece = bP;
    for (int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score -= pawnTable[MIRROR_64(INDEX_120_TO_64(square))];
        if ((isolatedPawnMask[INDEX_120_TO_64(square)] & board->pawns[BLACK]) == 0) {
            score -= pawnIsolated;
        }
        if ((blackPassedMask[INDEX_120_TO_64(square)] & board->pawns[WHITE]) == 0) {
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
        // If no pawns on the file / none of our pawns on the file, add score
        if (!(board->pawns[BOTH] & fileBitMask[filesBoard[square]])) {
            score += rookOpenFile;
        } else if (!(board->pawns[WHITE] & fileBitMask[filesBoard[square]])) {
            score += rookSemiOpenFile;
        }
    }

    piece = bR;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score -= rookTable[MIRROR_64(INDEX_120_TO_64(square))];
        if (!(board->pawns[BOTH] & fileBitMask[filesBoard[square]])) {
            score -= rookOpenFile;
        } else if (!(board->pawns[BLACK] & fileBitMask[filesBoard[square]])) {
            score -= rookSemiOpenFile;
        }
    }

    piece = wQ;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        // If no pawns on the file / none of our pawns on the file, add score
        if (!(board->pawns[BOTH] & fileBitMask[filesBoard[square]])) {
            score += queenOpenFile;
        } else if (!(board->pawns[WHITE] & fileBitMask[filesBoard[square]])) {
            score += queenSemiOpenFile;
        }
    }

    piece = bQ;
    for(int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        if (!(board->pawns[BOTH] & fileBitMask[filesBoard[square]])) {
            score -= queenOpenFile;
        } else if (!(board->pawns[BLACK] & fileBitMask[filesBoard[square]])) {
            score -= queenSemiOpenFile;
        }
    }

    piece = wK;
    if (board->material[BLACK] <= ENDGAME_MATERIAL) {
        score += kingEndgame[INDEX_120_TO_64(board->pieceList[piece][0])];
    } else {
        score += kingOpening[INDEX_120_TO_64(board->pieceList[piece][0])];
    }

    piece = bK;
    if (board->material[WHITE] <= ENDGAME_MATERIAL) {
        score -= kingEndgame[MIRROR_64(INDEX_120_TO_64(board->pieceList[piece][0]))];
    } else {
        score -= kingOpening[MIRROR_64(INDEX_120_TO_64(board->pieceList[piece][0]))];
    }

    if (board->pieceCounts[wB] >= 2) score += bishopPair;
    if (board->pieceCounts[bB] >= 2) score -= bishopPair;

    return board->side == WHITE ? score : -score; // flip score if side is black
}
