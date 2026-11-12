#include <bit>
#include <cstdio>
#include "defs.h"

/// Finds first (least significant) bit in bitboard, clears it, and returns its index
int popBit(U64 *bitboard) {
    int index = std::countr_zero(*bitboard);
    *bitboard &= (*bitboard - 1);
    return index;
}

/// Returns number of bits in bitboard
int countBits(U64 bitboard) {
    return std::popcount(bitboard);
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

