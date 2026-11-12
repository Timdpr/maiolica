// FEN parsing: valid FENs parse unchanged, malformed ones return an error without touching memory
// they shouldn't (build with ASan/UBSan to prove the second part). Usage: fen_tests
#include "defs.h"
#include <cstdio>

static int failures = 0;

#define CHECK(cond, fen) \
    if (!(cond)) { failures++; std::printf("FAIL %-40s %s\n", #cond, fen); }

// every FEN used by tests/perft_tests.cpp and src/bench.cpp
static const char *validFens[] = {
    START_FEN,
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "K1k5/8/P7/8/8/8/8/8 w - - 0 1",
    "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1",
    "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",
    "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1",
    "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",
    "5k2/8/8/8/8/8/8/4K2R w K - 0 1",
    "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1",
    "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1",
    "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1",
    "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1",
    "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1",
    "4k3/1P6/8/8/8/8/K7/8 w - - 0 1",
    "8/P1k5/K7/8/8/8/8/8 w - - 0 1",
    "8/k1P5/8/1K6/8/8/8/8 w - - 0 1",
    "r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 1",
    "2r3k1/pp3ppp/4p3/3pP3/3P4/P4N2/1P3PPP/2R3K1 b - - 0 25",
    "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1",
    "6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1",
};

static const char *badFens[] = {
    "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",           // 9 empty squares
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNRR w KQkq - 0 1",          // long rank
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNRRRRRRRRRRRR w KQkq - 0 1", // way past h1
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN w KQkq - 0 1",            // short rank
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/8 w KQkq - 0 1",         // 9 ranks
    "rnbqkbnr/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",             // 7 ranks
    "rnbqkbnr/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1",             // 7 ranks, 'b' looks like a bishop
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNX w KQkq - 0 1",           // bad piece char
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR KQkq - 0 1",             // missing side
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",           // bad side
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",                        // truncated after board
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR ",                       // truncated after the space
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB",                             // truncated mid-rank
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w kq - 0 1",             // no white king
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/ w KQkq - 0 1",          // trailing slash
    "kk6/8/8/8/8/8/8/4K3 w - - 0 1",                                      // two black kings
    "4k3/8/8/8/8/8/NNNNNNNN/NNN1K3 w - - 0 1",                            // 11 white knights
    "4k3/8/8/8/8/8/QQQQQQQQ/QQQQQQQK w - - 0 1",                          // 15 white queens
    "P3k3/8/8/8/8/8/8/4K3 w - - 0 1",                                     // pawn on rank 8
    "4k3/8/8/8/8/8/8/p3K3 w - - 0 1",                                     // pawn on rank 1
    "",                                                                   // empty string
    " ",
    "/",
    "8/8/8/8/8/8/8/8 w - - 0 1",                                          // no kings at all
};

int main() {
    initAll();

    for (const char *fen : validFens) {
        Board board{};
        CHECK(parseFen(fen, board) == 0, fen)
    }

    for (const char *fen : badFens) {
        Board board{};
        CHECK(parseFen(fen, board) != 0, fen)
    }

    // the same position with and without clocks (and castling/ep fields) builds the same board
    {
        Board a{}, b{}, c{};
        const char *full = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
        const char *noClock = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
        const char *bare = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w";
        CHECK(parseFen(full, a) == 0, full)
        CHECK(parseFen(noClock, b) == 0, noClock)
        CHECK(a.positionKey == b.positionKey && b.fiftyMove == 0 && b.historyPly == 0, noClock)
        CHECK(parseFen(bare, c) == 0 && c.side == WHITE && c.castlingPerms == 0 && c.enPasSq == NO_SQ, bare)
    }

    // halfmove clock lands in fiftyMove (clamped to 100); the fullmove number leaves historyPly alone
    {
        Board board{};
        const char *fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 37 80";
        CHECK(parseFen(fen, board) == 0 && board.fiftyMove == 37 && board.historyPly == 0, fen)
        fen = "4k3/8/8/8/8/8/8/4K3 b - - 99999999999999999999 1";
        CHECK(parseFen(fen, board) == 0 && board.fiftyMove == 100 && board.side == BLACK, fen)
        fen = "4k3/8/8/8/8/8/8/4K3 w - - x 1";
        CHECK(parseFen(fen, board) == 0 && board.fiftyMove == 0, fen)
    }

    // a castling right without its king and rook is dropped: only the 5 king moves remain
    {
        Board board{};
        const char *fen = "4k3/8/8/8/8/8/8/4K3 w K -";
        CHECK(parseFen(fen, board) == 0 && board.castlingPerms == 0, fen)
        leafNodes = 0;
        perft(1, board);
        CHECK(leafNodes == 5, fen)
        CHECK(board.pawns[BOTH] == 0, fen)
        fen = "r3k2r/8/8/8/8/8/8/R3K1R1 w KQkq - 0 1"; // h1 rook moved to g1
        CHECK(parseFen(fen, board) == 0 && board.castlingPerms == (wQ_CA | bK_CA | bQ_CA), fen)
    }

    // a bogus en passant square is ignored, a real one is kept
    {
        Board board{}, clean{};
        const char *fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e4 0 1";
        CHECK(parseFen(fen, board) == 0 && board.enPasSq == NO_SQ, fen)
        fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e3 0 1"; // rank 3 but white to move
        CHECK(parseFen(fen, board) == 0 && board.enPasSq == NO_SQ, fen)
        fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq z9 0 1";
        CHECK(parseFen(fen, board) == 0 && board.enPasSq == NO_SQ, fen)
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq e3 0 1"; // no pawn on e4
        CHECK(parseFen(fen, board) == 0 && board.enPasSq == NO_SQ, fen)
        CHECK(parseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", clean) == 0
              && clean.positionKey == board.positionKey, fen)
        fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
        CHECK(parseFen(fen, board) == 0 && board.enPasSq == E3, fen)
        fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e";       // truncated ep
        CHECK(parseFen(fen, board) == 0 && board.enPasSq == NO_SQ, fen)
    }

    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
