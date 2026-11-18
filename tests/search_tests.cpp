// A short search, for debug and sanitiser builds. Perft never makes a null move, and bench is too slow there.
// So this is where the ASSERTs in makeMove, isInCheck and checkBoard meet a real search. Usage: search_tests
#include "defs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const struct { const char *name, *fen; int depth; } cases[] = {
    {"startpos", START_FEN, 6},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4},
    {"pins", "2kr4/b7/1B1R1b2/4P3/rP1K1Q1q/8/1R1N1B2/b2r2q1 w - - 0 1", 5},
    {"en-passant", "r3k2r/1pp2pp1/8/pP1pPpP1/P2P1P1p/8/7P/R3K2R w KQkq a6 0 1", 6},
    {"rook-ending", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7},
};

// The ponder reply comes straight out of the table, where an entry can be stale or belong to another position.
// So plant one that isn't a move in the position (it must be refused), then one that is (it must come back)
static bool ponderReplyIsChecked() {
    Board board{};
    parseFen(START_FEN, board);
    const int e4 = parseMove("e2e4", board);
    Board next = board;
    if (!makeMove(next, e4)) return false;
    clearHashTable(hashTable);
    storeHashEntry(next, hashTable, e4, 0, HF_EXACT, 1); // white's move, in a position where it's black to play
    const bool refused = probePonderMove(board, e4, hashTable) == NO_MOVE;
    const int e5 = parseMove("e7e5", next);
    storeHashEntry(next, hashTable, e5, 0, HF_EXACT, 1);
    return refused && e5 != NO_MOVE && probePonderMove(board, e4, hashTable) == e5;
}

// An entry is packed into one 64-bit word, so store the widest value of every field & see that each comes back:
// both mate scores, the deepest depth, every kind of score, a castling move & a capturing promotion
static bool entryReadsBack() {
    Board board{};
    parseFen("r3k2r/1P6/8/8/8/8/8/R3K2R w KQkq - 0 1", board);
    const int moves[] = {parseMove("e1g1", board), parseMove("b7a8q", board)};
    const int scores[] = {ALPHABETA_BOUND - 1, -ALPHABETA_BOUND + 1, IS_MATE, -IS_MATE, 12345, 0, -1};
    bool ok = moves[0] != NO_MOVE && moves[1] != NO_MOVE;
    for (const int move : moves) {
        for (const int score : scores) {
            for (const int flags : {HF_ALPHA, HF_BETA, HF_EXACT}) {
                int gotMove = NO_MOVE, gotScore = 0;
                clearHashTable(hashTable);
                storeHashEntry(board, hashTable, move, score, flags, MAX_DEPTH - 1);
                // a window with the score on its edge: an upper bound answers at alpha, a lower bound at beta
                const int alpha = flags == HF_BETA ? score - 1 : score;
                const int beta = flags == HF_BETA ? score : score + 1;
                ok = ok && probeHashEntry(board, hashTable, &gotMove, &gotScore, alpha, beta, MAX_DEPTH - 1)
                     && gotMove == move && gotScore == score && probePVTable(board, hashTable) == move;
                // & a window the bound says nothing about isn't answered (an exact score always is)
                const bool answered = flags == HF_BETA ? probeHashEntry(board, hashTable, &gotMove, &gotScore, score, score + 1, 1)
                                                       : probeHashEntry(board, hashTable, &gotMove, &gotScore, score - 1, score, 1);
                ok = ok && answered == (flags == HF_EXACT);
            }
        }
    }
    // depth: an entry one ply short of what's asked for gives its move but no score
    int gotMove = NO_MOVE, gotScore = 0;
    clearHashTable(hashTable);
    storeHashEntry(board, hashTable, moves[0], 0, HF_EXACT, MAX_DEPTH - 2);
    ok = ok && !probeHashEntry(board, hashTable, &gotMove, &gotScore, -1, 1, MAX_DEPTH - 1) && gotMove == moves[0];
    // age: a deeper entry from this search is kept, one from an earlier search is replaced. Also when the age wraps
    for (const int age : {0, 0x3FFF}) {
        clearHashTable(hashTable);
        hashTable.currentAge = age;
        storeHashEntry(board, hashTable, moves[0], 0, HF_EXACT, 10);
        storeHashEntry(board, hashTable, moves[1], 0, HF_EXACT, 1);
        ok = ok && probePVTable(board, hashTable) == moves[0];
        hashTable.currentAge++;
        storeHashEntry(board, hashTable, moves[1], 0, HF_EXACT, 1);
        ok = ok && probePVTable(board, hashTable) == moves[1];
    }
    clearHashTable(hashTable);
    return ok;
}

// The move played must be the search's own. Its entry for the position it was asked about can be refused by the
// table, when a deeper one from the same search is already there (with several threads, another thread's). So plant
// one, saying only 'nothing here scores above 0' & carrying a move that throws the mate away, & see what gets played
static char bestMoveLine[32];
static bool rootMoveIsTheSearchsOwn() {
    Board board{};
    SearchInfo info{};
    parseFen("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", board); // Ra8 mates
    clearHashTable(hashTable);
    hashTable.currentAge++; // the age the search is about to give its entries
    storeHashEntry(board, hashTable, parseMove("a1a2", board), 0, HF_ALPHA, 40);
    hashTable.currentAge--;
    info.depth = 3;
    info.startTime = getTimeMS();
    lineHandler = [](const char *line) {
        if (!std::strncmp(line, "bestmove ", 9)) std::snprintf(bestMoveLine, sizeof(bestMoveLine), "%s", line + 9);
    };
    searchPosition(board, info, hashTable);
    lineHandler = [](const char *) {};
    clearHashTable(hashTable);
    return !std::strncmp(bestMoveLine, "a1a8", 4);
}

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
        nullCuts += board.nullCut;
        std::printf("%-11s d%d %7lld nodes %4d null-move cutoffs %5lld ms\n",
                    c.name, c.depth, info.nodes, board.nullCut, (long long)(getTimeMS() - info.startTime));
    }
    const bool ponderOk = ponderReplyIsChecked();
    const bool entryOk = entryReadsBack();
    const bool rootOk = rootMoveIsTheSearchsOwn();
    free(hashTable.hTable);

    // a failed ASSERT aborts, so getting here is most of the test. But make sure null moves were part of it
    const bool ok = nullCuts > 0;
    std::printf("%lld nodes, %d null-move cutoffs: %s\n", totalNodes, nullCuts, ok ? "ok" : "FAIL, no null moves were searched");
    if (!ponderOk) std::printf("FAIL, the ponder reply from the table isn't checked against the position\n");
    if (!entryOk) std::printf("FAIL, a table entry didn't read back as it was stored\n");
    if (!rootOk) std::printf("FAIL, the move played came from a table entry that wasn't the search's own: bestmove %s\n", bestMoveLine);
    return ok && ponderOk && entryOk && rootOk ? 0 : 1;
}
