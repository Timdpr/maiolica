#include "defs.h"

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

/// Use to get square index for black, as above tables are designed for white
const int mirror64[64] = {
        56	,	57	,	58	,	59	,	60	,	61	,	62	,	63	,
        48	,	49	,	50	,	51	,	52	,	53	,	54	,	55	,
        40	,	41	,	42	,	43	,	44	,	45	,	46	,	47	,
        32	,	33	,	34	,	35	,	36	,	37	,	38	,	39	,
        24	,	25	,	26	,	27	,	28	,	29	,	30	,	31	,
        16	,	17	,	18	,	19	,	20	,	21	,	22	,	23	,
        8	,	9	,	10	,	11	,	12	,	13	,	14	,	15	,
        0	,	1	,	2	,	3	,	4	,	5	,	6	,	7
};

/// Get square index for black from white, as eval tables are designed for white
#define MIRROR_64(square) (mirror64[(square)])

/// Evaluate a board position! @return a score (in 100ths of a pawn)
int evalPosition(const Board *board) {
    int score = board->material[WHITE] - board->material[BLACK]; // compare material score (remember white is +vs, black -ve!)

    // Get scores from tables for each piece type:
    int piece = wP;
    for (int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) { // for as many pieces of this type that are on the board...
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score += pawnTable[INDEX_120_TO_64(square)];
    }

    piece = bP;
    for (int pieceNum = 0; pieceNum < board->pieceCounts[piece]; ++pieceNum) {
        int square = board->pieceList[piece][pieceNum];
        ASSERT(squareOnBoard(square))
        score -= pawnTable[MIRROR_64(INDEX_120_TO_64(square))];
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