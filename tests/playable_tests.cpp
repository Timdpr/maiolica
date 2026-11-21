// movePlayable: 'could this move be played here?', asked of a move that didn't come from the move generator.
// It must say yes to exactly the moves generateAllMoves lists. A move is 25 bits, so in each position every
// one of the 33 million possible moves can be asked about, and the answer checked against the list.
// Usage: playable_tests [quick|smoke]  (smoke asks about a spread of them: for debug & sanitiser builds)
#include "defs.h"
#include <cstdio>
#include <cstring>
#include <vector>

static const int MOVE_BITS = 25;
static int failures = 0;

static const char *fens[] = {
    START_FEN,
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",    // every kind of move but en passant
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/Pp2P3/2N2Q1p/1PPBBPPP/R3K2R b KQkq a3 0 1",   // en passant for black
    "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",           // en passant for white, one pawn of two can take
    "rnbqkbnr/pp2pppp/8/2ppP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3",
    "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                                    // castling, both ways, both sides
    "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
    "r3k2r/8/8/8/8/8/8/R3K2R w Kq - 0 1",                                      // ...with some of the rights gone
    "r3k2r/8/8/8/8/5r2/8/R3K2R w KQkq - 0 1",                                  // f1 attacked: no castling short
    "r3k2r/8/8/8/8/3r4/8/R3K2R w KQkq - 0 1",                                  // d1 attacked: none long
    "r3k2r/8/8/8/8/1r6/8/R3K2R w KQkq - 0 1",                                  // b1 attacked: long is still fine
    "r3k2r/8/8/8/8/4r3/8/R3K2R w KQkq - 0 1",                                  // in check: none at all
    "rn2k1nr/8/8/8/8/8/8/RN2K1NR w KQkq - 0 1",                                // a piece in the way
    "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N w - - 0 1",                                 // promotions, straight on & by capture
    "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
    "4k3/P6P/8/8/8/8/p6p/4K3 w - - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                               // pins & double pushes
    "8/8/8/8/8/1k6/2q5/K7 w - - 0 1",                                          // in check with one way out
    "rnb1kbnr/pppp1ppp/8/4p3/5PPq/8/PPPPP2P/RNBQKBNR w KQkq - 1 3",            // checkmated: the list is still full of moves
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "4k3/8/8/8/8/8/8/4K3 w - - 0 1",
    "7k/8/8/q7/8/8/8/R3K2R w KQ - 0 1",
};

/// One position: is movePlayable's answer the list's, for every move asked about?
static void check(const Board& board, const char *name, bool everyMove) {
    MoveList list;
    generateAllMoves(board, list);
    std::vector<char> inList(size_t(1) << MOVE_BITS, 0);
    for (int i = 0; i < list.count; ++i) {
        inList[list.moves[i].move] = 1;
    }
    int wrong = 0;
    auto ask = [&](int move) {
        bool expected = move >= 0 && move < (1 << MOVE_BITS) && inList[move];
        if (bool(movePlayable(board, move)) != expected) {
            if (wrong++ < 5) {
                std::printf("FAIL %s: move %#x (from %d to %d) is %s the list, but movePlayable says %s\n", name, move,
                            GET_FROM(move), GET_TO(move), expected ? "in" : "not in", expected ? "no" : "yes");
            }
        }
    };
    // every move in the list, the same move with any one bit changed, & with a bit set that no move uses
    for (int i = 0; i < list.count; ++i) {
        ask(list.moves[i].move);
        for (int bit = 0; bit < 31; ++bit) {
            ask(list.moves[i].move ^ (1 << bit));
        }
    }
    ask(NO_MOVE);
    // then every move there could be, or every 97th of them
    for (int move = 0; move < (1 << MOVE_BITS); move += everyMove ? 1 : 97) {
        ask(move);
    }
    failures += wrong > 0;
}

int main(int argc, char **argv) {
    initAll();
    bool everyMove = argc < 2 || std::strcmp(argv[1], "smoke") != 0;

    for (const char *fen : fens) {
        Board board{};
        if (parseFen(fen, board) != 0) { failures++; std::printf("FAIL bad fen %s\n", fen); continue; }
        check(board, fen, everyMove);
        mirrorBoard(board); // & the same with the colours swapped
        check(board, fen, everyMove);
    }

    if (failures) {
        std::printf("%d position(s) failed\n", failures);
        return 1;
    }
    std::printf("movePlayable agrees with the move list in %d positions (%s)\n", int(2 * sizeof(fens) / sizeof(fens[0])),
                everyMove ? "every move asked about" : "a spread of moves asked about");
    return 0;
}
