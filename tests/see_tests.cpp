// Static exchange evaluation: captures whose exchange value is known, then every capture in a set of busy
// positions against a slow second opinion that works the exchange out the long way. Usage: see_tests
#include "defs.h"
#include <algorithm>
#include <climits>
#include <cstdio>

static int failures = 0;

// Pawn 100, knight & bishop 325, rook 550, queen 1000 (pieceValues). A value is what the side making the capture
// comes out ahead by, with both sides free to stop taking
static const struct { const char *name, *fen, *move; int value; } cases[] = {
    {"a loose pawn", "1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - 0 1", "e1e5", 100},
    {"knight for a pawn, everything else lined up behind", "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1", "d3e5", -225},
    {"pawn takes pawn, pawn takes back", "4k3/8/3p4/4p3/3P4/8/8/4K3 w - - 0 1", "d4e5", 0},
    {"queen takes a defended pawn", "4k3/8/3p4/4p3/8/8/4Q3/4K3 w - - 0 1", "e2e5", -900},
    {"a pawn defends forwards only", "4k3/8/8/4p3/3p4/8/8/4RK2 w - - 0 1", "e1e5", 100},
    {"bishop takes knight, pawn takes back (black)", "4k3/8/8/3b4/8/5N2/6P1/6K1 b - - 0 1", "d5f3", 0},
    {"knight for a pawn, & a pawn back", "4k3/8/3p4/4p3/8/5N2/8/4RK2 w - - 0 1", "f3e5", -125},
    // pieces lined up behind one another
    {"rook behind rook", "4k3/4r3/8/4p3/8/8/4R3/4RK2 w - - 0 1", "e2e5", 100},
    {"two rooks each, the pawn defended", "3rk3/3r4/8/3p4/8/8/3R4/3RK3 w - - 0 1", "d2d5", -450},
    {"rook behind queen", "4k3/4r3/8/4p3/8/8/4Q3/4RK2 w - - 0 1", "e2e5", -350},
    {"bishop behind the pawn that takes", "4k3/8/2p5/3p4/4P3/5B2/8/4K3 w - - 0 1", "e4d5", 100},
    {"their rook behind our queen", "4k3/8/8/4p3/8/8/4Q2K/4r3 w - - 0 1", "e2e5", -900},
    {"queen behind bishop, on a diagonal (black)", "4k3/8/8/7q/8/5b2/4R3/K5N1 b - - 0 1", "f3e2", 550},
    // en passant: the pawn taken isn't on the square, & leaves its file open
    {"en passant", "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6", 100},
    {"en passant, pawn takes back", "4k3/2p5/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6", 0},
    {"en passant, rook behind the pawn taken", "4k3/2p5/8/3pP3/8/8/8/3RK3 w - d6 0 1", "e5d6", 100},
    {"en passant (black)", "4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1", "e4d3", 100},
    // promotions: the pawn turns into what can be taken back
    {"takes & queens", "3r4/4P3/8/8/8/8/8/k3K3 w - - 0 1", "e7d8q", 1450},
    {"takes & queens, king takes back", "2kr4/4P3/8/8/8/8/8/4K3 w - - 0 1", "e7d8q", 450},
    {"takes & knights, king takes back", "2kr4/4P3/8/8/8/8/8/4K3 w - - 0 1", "e7d8n", 450},
    // ...& without taking anything: the search asks about these too, to sort a queening move
    {"queens on a safe square", "8/4P3/8/8/8/8/8/k3K3 w - - 0 1", "e7e8q", 900},
    {"queens where a rook takes it", "3r4/4P3/8/8/8/8/8/k3K3 w - - 0 1", "e7e8q", -100},
    {"queens, rook takes, the rook behind the pawn takes back", "3r4/4P3/8/8/8/8/8/k3RK2 w - - 0 1", "e7e8q", 450},
    {"queens where a rook takes it (black)", "K3k3/8/8/8/8/8/4p3/3R4 b - - 0 1", "e2e1q", -100},
    // kings: one takes only when nothing can take it back
    {"king takes a loose pawn", "4k3/8/8/8/8/8/4p3/4K3 w - - 0 1", "e1e2", 100},
    {"only the king defends", "8/8/8/8/3k4/3p4/8/3R2K1 w - - 0 1", "d1d3", -450},
    {"the king defends, but can't take back", "8/8/8/8/3k4/3p4/8/3R1BK1 w - - 0 1", "d1d3", 100},
};

// -- The second opinion: the exchange played out on a copy of the squares, one capture at a time.
// Where a side has two least valuable attackers to choose from, both are tried, so the answer is a range
static const int knightSteps[8] = { -8, -19, -21, -12, 8, 19, 21, 12 };
static const int rookSteps[4] = { -1, -10, 1, 10 };
static const int bishopSteps[4] = { -9, -11, 11, 9 };

static bool attacks(const int *pieces, int from, int to) {
    int piece = pieces[from], difference = to - from;
    if (piece == wP) return difference == 9 || difference == 11;
    if (piece == bP) return difference == -9 || difference == -11;
    if (IS_KNIGHT(piece)) return std::find(knightSteps, knightSteps + 8, difference) != knightSteps + 8;
    if (IS_KING(piece)) {
        return std::find(rookSteps, rookSteps + 4, difference) != rookSteps + 4
               || std::find(bishopSteps, bishopSteps + 4, difference) != bishopSteps + 4;
    }
    if (IS_ROOK_OR_QUEEN(piece)) {
        for (int step : rookSteps) {
            for (int square = from + step; ; square += step) {
                if (square == to) return true;
                if (pieces[square] != EMPTY) break;
            }
        }
    }
    if (IS_BISHOP_OR_QUEEN(piece)) {
        for (int step : bishopSteps) {
            for (int square = from + step; ; square += step) {
                if (square == to) return true;
                if (pieces[square] != EMPTY) break;
            }
        }
    }
    return false;
}

struct Range { int low, high; };

/// What 'side' can win on the square by taking there, or 0 if it does better not to
static Range exchange(int *pieces, int to, int side) {
    int least = INT_MAX;
    for (int square = A1; square <= H8; ++square) {
        int piece = pieces[square];
        if (piece >= wP && piece <= bK && pieceColours[piece] == side && attacks(pieces, square, to)) {
            least = std::min(least, pieceValues[piece]);
        }
    }
    if (least == INT_MAX) {
        return {0, 0};
    }
    Range best = {INT_MAX, INT_MIN};
    for (int square = A1; square <= H8; ++square) {
        int piece = pieces[square];
        if (piece >= wP && piece <= bK && pieceColours[piece] == side && pieceValues[piece] == least && attacks(pieces, square, to)) {
            int taken = pieces[to];
            pieces[to] = piece;
            pieces[square] = EMPTY;
            Range reply = exchange(pieces, to, side ^ 1);
            pieces[square] = piece;
            pieces[to] = taken;
            best.low = std::min(best.low, std::max(0, pieceValues[taken] - reply.high));
            best.high = std::max(best.high, std::max(0, pieceValues[taken] - reply.low));
        }
    }
    return best;
}

static Range exchangeAfter(const Board& board, int move) {
    int pieces[BRD_SQ_NUM];
    std::copy(board.pieces, board.pieces + BRD_SQ_NUM, pieces);
    int from = GET_FROM(move), to = GET_TO(move), promoted = GET_PROMOTED(move);
    int wins = (move & MFLAG_EN_PASSANT) ? pieceValues[wP] : pieceValues[GET_CAPTURED(move)];
    if (promoted != EMPTY) wins += pieceValues[promoted] - pieceValues[wP];
    if (move & MFLAG_EN_PASSANT) pieces[board.side == WHITE ? to - 10 : to + 10] = EMPTY;
    pieces[to] = promoted != EMPTY ? promoted : pieces[from];
    pieces[from] = EMPTY;
    Range reply = exchange(pieces, to, board.side ^ 1);
    return {wins - reply.high, wins - reply.low};
}

// seeAtLeast only says yes or no. Its value is the highest threshold it says yes to (or 'below FLOOR')
constexpr int FLOOR = -3000, CEILING = 3000;
static int seeValue(const Board& board, int move) {
    if (!seeAtLeast(board, move, FLOOR)) return FLOOR - 1;
    int yes = FLOOR, no = CEILING + 1;
    while (no - yes > 1) {
        int middle = (yes + no) / 2;
        if (seeAtLeast(board, move, middle)) yes = middle; else no = middle;
    }
    return yes;
}

// every capture here is compared with the second opinion, for the side to move & then with the colours swapped
static const char *busyFens[] = {
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "2kr4/b7/1B1R1b2/4P3/rP1K1Q1q/8/1R1N1B2/b2r2q1 w - - 0 1",
    "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 1",
    "3r1rk1/pp3ppp/2n1b3/q2pP3/3Nn3/2P1BN2/PP2QPPP/R4RK1 b - - 0 1",
    "r2q1rk1/1b1nbppp/p2ppn2/1p4B1/3NPP2/2N2Q2/PPP3PP/2KR1B1R w - - 0 1",
    "2r2rk1/1bqnbppp/p2ppn2/1p6/3NPP2/P1N1BQ2/1PP1B1PP/3R1R1K b - - 0 1",
    "r3k2r/1pp2pp1/8/pP1pPpP1/P2P1P1p/8/7P/R3K2R w KQkq a6 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "q3k2q/1P4P1/8/3n4/8/2B2R2/8/Q3K2Q w - - 0 1",
    "4k3/8/2n1b3/3p4/2P1P3/3B1N2/3R4/3QK3 w - - 0 1",
};

static int compareAll(const Board& board, const char *fen) {
    MoveList list;
    generateAllCaptureMoves(board, list);
    for (int i = 0; i < list.count; ++i) {
        int move = list.moves[i].move;
        Range slow = exchangeAfter(board, move);
        int quick = seeValue(board, move);
        if (quick < std::max(slow.low, FLOOR - 1) || quick > std::max(slow.high, FLOOR - 1)) {
            failures++;
            std::printf("FAIL %s gives %d, the long way gives %d to %d: %s\n", printMove(move), quick, slow.low, slow.high, fen);
        }
    }
    return list.count;
}

int main() {
    initAll();

    for (const auto& c : cases) {
        Board board{};
        if (parseFen(c.fen, board) != 0) { failures++; std::printf("FAIL bad fen %s\n", c.fen); continue; }
        int move = parseMove(c.move, board);
        if (move == NO_MOVE) { failures++; std::printf("FAIL bad move %s in %s\n", c.move, c.fen); continue; }
        if (!seeAtLeast(board, move, c.value) || seeAtLeast(board, move, c.value + 1)) {
            failures++;
            std::printf("FAIL %s: %s should be worth %d, but is worth %d\n", c.name, c.move, c.value, seeValue(board, move));
        }
        Range slow = exchangeAfter(board, move); // the second opinion had better agree with the table too
        if (slow.low != c.value || slow.high != c.value) {
            failures++;
            std::printf("FAIL %s: the long way gives %d to %d, not %d\n", c.name, slow.low, slow.high, c.value);
        }
    }

    // a king can't take a defended piece at all, so no threshold is low enough
    Board board{};
    parseFen("4k3/8/8/8/8/3p4/4p3/4K3 w - - 0 1", board);
    if (seeAtLeast(board, parseMove("e1e2", board), FLOOR)) {
        failures++;
        std::printf("FAIL the king took a defended pawn\n");
    }

    int compared = 0;
    for (const char *fen : busyFens) {
        if (parseFen(fen, board) != 0) { failures++; std::printf("FAIL bad fen %s\n", fen); continue; }
        compared += compareAll(board, fen);
        mirrorBoard(board);
        compared += compareAll(board, fen);
    }

    std::printf("%d captures compared, %d failure(s)\n", compared, failures);
    return failures ? 1 : 0;
}
