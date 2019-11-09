#include <cstdio>
#include "defs.h"

const int bitTable[64] = { // https://www.chessprogramming.org/Looking_for_Magics
        63, 30, 3, 32, 25, 41, 22, 33, 15, 50, 42, 13, 11, 53, 19, 34, 61, 29, 2,
        51, 21, 43, 45, 10, 18, 47, 1, 54, 9, 57, 0, 35, 62, 31, 40, 4, 49, 5, 52,
        26, 60, 6, 23, 44, 46, 27, 56, 16, 7, 39, 48, 24, 59, 14, 12, 55, 38, 28,
        58, 20, 37, 17, 36, 8
};

/// Finds first (least significant) bit in bitboard, and returns index
int popBit(U64 *bitboard) {
    U64 b = *bitboard ^ (*bitboard - 1);
    unsigned int fold = (unsigned) ((b & 0xffffffff) ^ (b >> 32));
    *bitboard &= (*bitboard - 1);
    return bitTable[(fold * 0x783a9b23) >> 26];
}

/// Returns number of bits in bitboard
int countBits(U64 bitboard) {
    int r;
    for (r = 0; bitboard; r++, bitboard &= bitboard - 1);
    return r;
}

void printBitBoard(U64 bitBoard) {
    auto shiftMe = U64(1);

    int square; // 120
    int square64; // 64

    std::printf("\n");
    for (int rank = RANK_8; rank >= RANK_1; --rank) {
        for (int file = FILE_A; file <= FILE_H; ++file) {
            square = FILE_RANK_TO_SQUARE_INDEX(file, rank);
            square64 = INDEX_120_TO_64(square);

            if ((shiftMe << square64) & bitBoard) {
                std::printf("X ");
            } else {
                std::printf("- ");
            }
        }
        std::printf("\n");
    }
    std::printf("\n\n");
}

