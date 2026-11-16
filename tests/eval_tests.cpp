// Eval: a position scores the same as its colour-flipped twin, the pawnless-draw shortcut is for
// positions with no pawns, and the king changes table when the other side runs low. Usage: eval_tests
#include "defs.h"
#include <cstdio>

static int failures = 0;

#define CHECK(cond, fen) \
    if (!(cond)) { failures++; std::printf("FAIL %-40s %s\n", #cond, fen); }

// pawns for both sides, for one side only & for neither; drawish material and not; an en passant square
static const char *mirrorFens[] = {
    START_FEN,
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "2r3k1/pp3ppp/4p3/3pP3/3P4/P4N2/1P3PPP/2R3K1 b - - 0 25",
    "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1",
    "4k3/4p3/8/8/8/8/8/4K3 w - - 0 1",
    "4k3/8/8/8/8/8/4P3/4KN2 w - - 0 1",
    "4kn2/4p3/8/8/8/8/8/4K3 w - - 0 1",
    "4k3/8/8/8/8/8/PP6/1K1B4 b - - 0 1",
    "1k1b4/pp6/8/8/8/8/8/4K3 b - - 0 1",
    "3rk3/8/8/8/8/8/5PPP/3R2K1 w - - 0 1",
    "3r2k1/5ppp/8/8/8/8/8/3RK3 w - - 0 1",
    "4k3/8/8/8/8/8/8/4KN2 w - - 0 1",
    "4kb2/8/8/8/8/8/8/2B1K3 b - - 0 1",
    "3rk3/8/8/8/8/8/8/3RK3 w - - 0 1",
    "4k3/8/8/8/8/8/8/3QK3 w - - 0 1",
    "3qk3/8/8/8/8/8/8/4K3 w - - 0 1",
    "4k3/8/8/8/8/8/8/2B1KB2 b - - 0 1",
    "8/8/4k3/8/3K4/8/P7/8 w - - 0 1",
    "8/p7/8/3k4/8/4K3/8/8 w - - 0 1",
};

static int eval(const char *fen) {
    Board board{};
    if (parseFen(fen, board) != 0) { failures++; std::printf("FAIL bad fen %s\n", fen); return 0; }
    return evalPosition(board);
}

int main() {
    initAll();

    // swap the colours & flip the board: the side to move sees the same score
    for (const char *fen : mirrorFens) {
        Board board{};
        CHECK(parseFen(fen, board) == 0, fen)
        int score = evalPosition(board);
        mirrorBoard(board);
        CHECK(evalPosition(board) == score, fen)
    }

    // no pawns and nothing to mate with is a draw, whoever has the extra piece
    CHECK(eval("4k3/8/8/8/8/8/8/4KN2 w - - 0 1") == 0, "KN v K")
    CHECK(eval("4kn2/8/8/8/8/8/8/4K3 w - - 0 1") == 0, "K v KN")
    CHECK(eval("3rk3/8/8/8/8/8/8/3RK3 w - - 0 1") == 0, "KR v KR")
    // but a pawn on either side means there's a game to play
    CHECK(eval("4k3/8/8/8/8/8/4P3/4KN2 w - - 0 1") > 0, "KNP v K")
    CHECK(eval("4kn2/4p3/8/8/8/8/8/4K3 w - - 0 1") < 0, "K v KNP")
    CHECK(eval("4k3/8/8/8/8/8/8/3QK3 w - - 0 1") > 0, "KQ v K")

    // in an ending the king belongs in the middle. With the heavy pieces still on, it belongs at home
    CHECK(eval("4k3/8/8/8/3K4/8/P7/8 w - - 0 1") > eval("4k3/8/8/8/8/8/P7/K7 w - - 0 1"), "king in an ending")
    CHECK(eval("4k3/8/8/8/3K4/8/P7/8 b - - 0 1") < eval("4k3/8/8/8/8/8/P7/K7 b - - 0 1"), "king in an ending, black to move")
    CHECK(eval("r2q1rk1/ppp2ppp/8/8/8/8/PPP2PPP/R2Q1RK1 w - - 0 1") > eval("r2q1rk1/ppp2ppp/8/8/3K4/8/PPP2PPP/R2Q1R2 w - - 0 1"),
          "king in a middlegame")

    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
