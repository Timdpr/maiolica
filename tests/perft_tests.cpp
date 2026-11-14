// Perft regression: known leaf counts from https://www.chessprogramming.org/Perft_Results
// and Peter Ellis Jones' list of tricky positions. Usage: perft_tests [smoke|quick|full]
#include "defs.h"
#include <cstdio>
#include <cstring>

enum Tier { SMOKE, QUICK, FULL };

#define KIWIPETE "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
#define POS3 "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
#define POS4 "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
#define POS4_MIRRORED "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1"
#define POS5 "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
#define POS6 "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
#define PINS "2kr4/b7/1B1R1b2/4P3/rP1K1Q1q/8/1R1N1B2/b2r2q1"

static const struct { const char *name, *fen; int depth; long long expected; Tier tier; } cases[] = {
    // smoke: small enough for debug/sanitiser builds, where ASSERT(checkBoard) runs at every node
    {"startpos", START_FEN, 4, 197281, SMOKE},
    {"kiwipete", KIWIPETE, 3, 97862, SMOKE},
    {"pos3", POS3, 4, 43238, SMOKE},
    {"pos4", POS4, 3, 9467, SMOKE},
    {"pos5", POS5, 3, 62379, SMOKE},
    {"pos6", POS6, 3, 89890, SMOKE},
    {"self-stalemate", "K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2217, SMOKE},
    {"double-check", "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527, SMOKE},
    // pins: makeMove only looks along the line a piece left, so here's a pinned piece on each of the king's 8 lines.
    // These seven are our own positions; the counts were checked against a second, separately written move generator
    {"pins-white-to-move", PINS " w - - 0 1", 3, 21212, SMOKE},
    {"pins-black-to-move", PINS " b - - 0 1", 3, 40577, SMOKE},
    {"ep-along-a-pin", "1b2k3/8/8/3pP3/8/8/7K/8 w - d6 0 1", 5, 25828, SMOKE},
    {"ep-off-a-pin", "4r1k1/8/8/3pP3/8/8/8/4K3 w - d6 0 1", 4, 8889, SMOKE},
    {"ep-uncovers-a-diagonal", "b3k3/8/8/3pP3/8/8/8/7K w - d6 0 1", 5, 10676, SMOKE}, // can't arise in a game, but a FEN can ask
    {"promote-while-pinned", "5r1b/6P1/8/8/3K4/8/8/k7 w - - 0 1", 4, 40889, SMOKE},
    {"far-apart-not-in-line", "7k/6N1/8/8/b7/8/2P5/3K4 w - - 0 1", 4, 7836, SMOKE}, // d1 & g7: see initLineSteps

    // quick: the default release run, about a second
    {"startpos", START_FEN, 5, 4865609, QUICK},
    {"kiwipete", KIWIPETE, 4, 4085603, QUICK},
    {"pos3", POS3, 6, 11030083, QUICK},
    {"pos4", POS4, 4, 422333, QUICK},
    {"pos4-mirrored", POS4_MIRRORED, 4, 422333, QUICK},
    {"pos5", POS5, 4, 2103487, QUICK},
    {"pos6", POS6, 4, 3894594, QUICK},
    {"illegal-ep-1", "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1134888, QUICK},
    {"illegal-ep-2", "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 6, 1015133, QUICK},
    {"ep-discovered", "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1440467, QUICK},
    {"short-castle-check", "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 6, 661072, QUICK},
    {"long-castle-check", "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 803711, QUICK},
    {"castle-rights", "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 4, 1274206, QUICK},
    {"castle-prevented", "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 4, 1720476, QUICK},
    {"promote-out-of-check", "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3821001, QUICK},
    {"discovered-check", "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 5, 1004658, QUICK},
    {"promote-to-check", "4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 6, 217342, QUICK},
    {"underpromote-check", "8/P1k5/K7/8/8/8/8/8 w - - 0 1", 6, 92683, QUICK},
    {"stalemate-checkmate", "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 7, 567584, QUICK},

    // full: deep runs, ~20s in release
    {"startpos", START_FEN, 6, 119060324, FULL},
    {"kiwipete", KIWIPETE, 5, 193690690, FULL},
    {"pos3", POS3, 7, 178633661, FULL},
    {"pos4", POS4, 5, 15833292, FULL},
    {"pos5", POS5, 5, 89941194, FULL},
    {"pos6", POS6, 5, 164075551, FULL},
    {"pins-white-to-move", PINS " w - - 0 1", 5, 31947172, FULL},
    {"pins-black-to-move", PINS " b - - 0 1", 5, 51056354, FULL},
};

int main(int argc, char *argv[]) {
    const char *arg = argc > 1 ? argv[1] : "quick";
    const Tier tier = !std::strcmp(arg, "smoke") ? SMOKE : !std::strcmp(arg, "full") ? FULL : QUICK;
    initAll();

    int failures = 0;
    long long totalNodes = 0;
    TimeMS start = getTimeMS();
    for (const auto& c : cases) {
        if (c.tier != tier) continue;
        Board board{};
        parseFen(c.fen, board);
        leafNodes = 0;
        TimeMS t = getTimeMS();
        perft(c.depth, board);
        const bool ok = leafNodes == c.expected;
        failures += !ok;
        totalNodes += leafNodes;
        std::printf("%-4s %-21s d%d %11lld (expected %11lld) %6lld ms\n", ok ? "ok" : "FAIL",
                    c.name, c.depth, leafNodes, c.expected, (long long)(getTimeMS() - t));
    }
    TimeMS elapsed = getTimeMS() - start + 1;
    std::printf("%lld leaf nodes in %lld ms (%lld nps), %d failure(s)\n",
                totalNodes, (long long)elapsed, totalNodes * 1000 / elapsed, failures);
    return failures ? 1 : 0;
}
