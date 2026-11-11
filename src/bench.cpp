#include "defs.h"
#include <cstdio>

/// Fixed positions & depths. The total node count is the 'bench signature':
/// a change that isn't meant to alter the search must not alter it!
static const struct { const char *fen; int depth; } benchPositions[] = {
    {START_FEN, 8},
    {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6}, // kiwipete
    {"r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 1", 6},   // WAC1
    {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 6},
    {"2r3k1/pp3ppp/4p3/3pP3/3P4/P4N2/1P3PPP/2R3K1 b - - 0 25", 9},
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 10},
    {"8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1", 18},                               // fine #70, pawn ending
    {"8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 10},                                     // minor piece ending
    {"6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", 8},                                       // back rank mate
};

/// Search each position to a fixed depth on a fresh board & cleared 16MB table, then print the totals
void bench() {
    initHashTable(hashTable, 16);
    long long totalNodes = 0;
    TimeMS start = getTimeMS();

    for (const auto& pos : benchPositions) {
        Board board{};
        SearchInfo info{};
        clearHashTable(hashTable);
        parseFen(pos.fen, board);
        info.depth = pos.depth;
        info.startTime = getTimeMS();
        searchPosition(board, &info, hashTable);
        totalNodes += info.nodes;
    }

    TimeMS elapsed = getTimeMS() - start + 1; // +1 so we never divide by zero
    std::printf("===========================\n");
    std::printf("Total time (ms) : %lld\n", (long long)elapsed);
    std::printf("Nodes searched  : %lld\n", totalNodes);
    std::printf("Nodes/second    : %lld\n", totalNodes * 1000 / elapsed);
    std::printf("%lld nodes %lld nps\n", totalNodes, totalNodes * 1000 / elapsed); // the line OpenBench reads
}
