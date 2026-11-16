// A short search, for debug and sanitiser builds. Perft never makes a null move, and bench is too slow there.
// So this is where the ASSERTs in makeMove, isInCheck and checkBoard meet a real search. Usage: search_tests
#include "defs.h"
#include <cstdio>
#include <cstdlib>

static const struct { const char *name, *fen; int depth; } cases[] = {
    {"startpos", START_FEN, 6},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4},
    {"pins", "2kr4/b7/1B1R1b2/4P3/rP1K1Q1q/8/1R1N1B2/b2r2q1 w - - 0 1", 5},
    {"en-passant", "r3k2r/1pp2pp1/8/pP1pPpP1/P2P1P1p/8/7P/R3K2R w KQkq a6 0 1", 6},
    {"rook-ending", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7},
};

int main() {
    initAll();
    initHashTable(hashTable, 16);
    lineHandler = [](const char *) {}; // the search's info lines aren't what we're here for

    long long totalNodes = 0;
    int nullCuts = 0;
    for (const auto& c : cases) {
        Board board{};
        SearchInfo info{};
        clearHashTable(hashTable);
        parseFen(c.fen, board);
        info.depth = c.depth;
        info.startTime = getTimeMS();
        searchPosition(board, info, hashTable);
        totalNodes += info.nodes;
        nullCuts += info.nullCut;
        std::printf("%-11s d%d %7lld nodes %4d null-move cutoffs %5lld ms\n",
                    c.name, c.depth, info.nodes, info.nullCut, (long long)(getTimeMS() - info.startTime));
    }
    free(hashTable.hTable);

    // a failed ASSERT aborts, so getting here is most of the test. But make sure null moves were part of it
    const bool ok = nullCuts > 0;
    std::printf("%lld nodes, %d null-move cutoffs: %s\n", totalNodes, nullCuts, ok ? "ok" : "FAIL, no null moves were searched");
    return ok ? 0 : 1;
}
