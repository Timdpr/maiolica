#include <random>
#include "defs.h"

std::random_device rd;
std::mt19937_64 gen(rd());
std::uniform_int_distribution<U64> dist(0, INT64_MAX);
#define RAND_64 dist(gen)

int sq120ToSq64[BRD_SQ_NUM];
int sq64ToSq120[64];

/// All 1s
U64 setMask[64];
/// All 0s
U64 clearMask[64];

// Initialised with random numbers
U64 pieceKeys[13][120]; // [pieces][squares]
U64 sideKey;
U64 castleKeys[16];

int filesBoard[BRD_SQ_NUM]; // holds the file index for a board position, 0 - 7 or 100 if offboard
int ranksBoard[BRD_SQ_NUM]; // holds the rank index for a board position, 0 - 7 or 100 if offboard

U64 fileBitMask[8]; // all '1's for the indexed file
U64 rankBitMask[8]; // all '1's for the indexed rank

U64 whitePassedMask[64]; // does the indexed white pawn have a 'clear run' to promotion?
U64 blackPassedMask[64]; // does the indexed black pawn have a 'clear run' to promotion?
U64 isolatedPawnMask[64]; // is the indexed pawn 'isolated'?

void initEvalMasks() {
    for (int square = 0; square < 8; ++square) {
        fileBitMask[square] = U64(0);
        rankBitMask[square] = U64(0);
    }
    // loop through 64-based bitboards, setting '1' for the file/rank
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        for (int file = FILE_A; file <= FILE_H; file++) {
            int square = rank * 8 + file;
            fileBitMask[file] |= (U64(1) << square);
            rankBitMask[rank] |= (U64(1) << square);
        }
    }
    for (int square = 0; square < 64; ++square) {
        isolatedPawnMask[square] = U64(0);
        whitePassedMask[square] = U64(0);
        blackPassedMask[square] = U64(0);
    }
    //
    for (int square = 0; square < 64; ++square) {
        int tsq = square + 8;

        while (tsq < 64) {
            whitePassedMask[square] |= (U64(1) << tsq);
            tsq += 8;
        }

        tsq = square - 8;
        while (tsq >= 0) {
            blackPassedMask[square] |= (U64(1) << tsq);
            tsq -= 8;
        }

        if (filesBoard[INDEX_64_TO_120(square)] > FILE_A) {
            isolatedPawnMask[square] |= fileBitMask[filesBoard[INDEX_64_TO_120(square)] - 1];

            tsq = square + 7;
            while (tsq < 64) {
                whitePassedMask[square] |= (U64(1) << tsq);
                tsq += 8;
            }

            tsq = square - 9;
            while (tsq >= 0) {
                blackPassedMask[square] |= (U64(1) << tsq);
                tsq -= 8;
            }
        }

        if (filesBoard[INDEX_64_TO_120(square)] < FILE_H) {
            isolatedPawnMask[square] |= fileBitMask[filesBoard[INDEX_64_TO_120(square)] + 1];

            tsq = square + 9;
            while (tsq < 64) {
                whitePassedMask[square] |= (U64(1) << tsq);
                tsq += 8;
            }

            tsq = square - 7;
            while (tsq >= 0) {
                blackPassedMask[square] |= (U64(1) << tsq);
                tsq -= 8;
            }
        }
    }
}

/// Initialise arrays of rank and file indexes
void initFilesRanksBoard() {
    for (int i = 0; i < BRD_SQ_NUM; ++i) {
        filesBoard[i] = OFFBOARD;
        ranksBoard[i] = OFFBOARD;
    }
    for (int rank = RANK_1; rank <= RANK_8; ++rank) {
        for (int file = FILE_A; file <= FILE_H; ++file) {
            int square = FILE_RANK_TO_SQUARE_INDEX(file, rank);
            filesBoard[square] = file;
            ranksBoard[square] = rank;
        }
    }
}

/// Initialise hash keys with random numbers
void initHashKeys() {
    for (int i = 0; i < 13; ++i) {
        for(int j = 0; j < 120; ++j) {
            pieceKeys[i][j] = RAND_64;
        }
    }
    sideKey = RAND_64;
    for (int i = 0; i < 16; ++i) {
        castleKeys[i] = RAND_64;
    }
}

void initBitMasks() {
    for (int i = 0; i < 64; i++) {
        setMask[i] = U64(0);
        clearMask[i] = U64(0);
    }
    for (int i = 0; i < 64; i++) {
        setMask[i] |= U64(1) << i; // shift 1 into every position
        clearMask[i] = ~setMask[i]; // bitwise complement: clearMask is the inverse of setMask!
    }
}

void initSquare120To64() {
    for (int &i : sq120ToSq64) {
        i = 65; // initialise with impossible values
    }
    for (int &i : sq64ToSq120) {
        i = 120; // initialise with impossible values
    }
    // Set up conversion arrays
    int square;
    int square64 = 0;
    for (int rank = RANK_1; rank <= RANK_8; ++rank) {
        for (int file = FILE_A; file <= FILE_H; ++file) {
            square = FILE_RANK_TO_SQUARE_INDEX(file, rank);
            sq64ToSq120[square64] = square;
            sq120ToSq64[square] = square64;
            square64++;
        }
    }
}

/// Initialise everything!
void initAll() {
    initSquare120To64();
    initBitMasks();
    initHashKeys();
    initFilesRanksBoard();
    initEvalMasks();
    initMVVLVA();
}