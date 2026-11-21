// Static exchange evaluation: captures whose exchange value is known, then every move in a set of busy
// positions against a slow second opinion that works the exchange out the long way. Both with pins seen &
// without. Usage: see_tests
#include "defs.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>

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
    // kings: one takes only when nothing can take it back
    {"king takes a loose pawn", "4k3/8/8/8/8/8/4p3/4K3 w - - 0 1", "e1e2", 100},
    {"only the king defends", "8/8/8/8/3k4/3p4/8/3R2K1 w - - 0 1", "d1d3", -450},
    {"the king defends, but can't take back", "8/8/8/8/3k4/3p4/8/3R1BK1 w - - 0 1", "d1d3", 100},
};

// Pins. Asked to see them, the routine knows that a piece of the other side's that is pinned to its king can't take
// back; not asked, it lets the piece take. 'blind' is the value then
static const struct { const char *name, *fen, *move; int value, blind; } pinCases[] = {
    {"the knight that defends is pinned", "7k/8/5n2/3p4/8/8/1B6/3R2K1 w - - 0 1", "d1d5", 100, -450},
    {"...and the same pawn when a pawn stands in the pin's way", "7k/6p1/5n2/3p4/8/8/1B6/3R2K1 w - - 0 1", "d1d5", -450, -450},
    {"a pinned pawn can't take back", "4k3/8/4p3/3p4/8/2N5/8/4RK2 w - - 0 1", "c3d5", 100, -225},
    {"a knight pinned on a file, the queen takes what it guards", "1k6/8/1n6/3p4/8/8/8/1R1Q2K1 w - - 0 1", "d1d5", 100, -900},
    {"a pinned defender (black to move)", "5rk1/1b6/8/8/5P2/8/6N1/7K b - - 0 1", "f8f4", 100, -450},
    {"two defenders, one of them pinned", "4k3/4n3/1n6/3p4/8/8/8/3RR1K1 w - - 0 1", "d1d5", -450, -450},
    {"pinned, but taking along the line of the pin", "7k/6b1/8/8/3p4/8/1B6/3R2K1 w - - 0 1", "d1d4", -125, -125},
    {"the piece that pinned it is the one that took", "4k3/3r4/8/8/Q2p4/8/8/4K3 w - - 0 1", "a4d4", -900, -900},
    {"the pin ends once the pinning piece has itself taken", "4k3/8/2n5/4p3/Q2p4/5N2/8/6K1 w - - 0 1", "f3d4", -225, -225},
    // the side that made the move may use its own pinned pieces: when the routine said no on their account, the move
    // turned out well many times as often as its other noes
    {"our own pinned piece still takes back", "6k1/8/6n1/b7/5p2/8/3B2N1/4K3 w - - 0 1", "g2f4", 100, 100},
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

/// Is the piece on 'square' pinned to its own king, as things stand on this copy of the board? Lined up with the king,
/// nothing between, & the next piece beyond it an enemy that moves along that line. The square of the exchange stops
/// the line (taking along the line of a pin is no pin), & a king that has itself taken is no longer anywhere to pin to
static bool pinned(const int *pieces, int square, int side, int to) {
    int king = NO_SQ;
    for (int sq = A1; sq <= H8; ++sq) {
        if (pieces[sq] == (side == WHITE ? wK : bK)) king = sq;
    }
    if (king == NO_SQ || king == to || king == square) return false;
    int files = filesBoard[square] - filesBoard[king], ranks = ranksBoard[square] - ranksBoard[king];
    if (files != 0 && ranks != 0 && std::abs(files) != std::abs(ranks)) return false;
    int step = (ranks > 0 ? 10 : ranks < 0 ? -10 : 0) + (files > 0 ? 1 : files < 0 ? -1 : 0);
    for (int sq = king + step; sq != square; sq += step) {
        if (sq == to || pieces[sq] != EMPTY) return false;
    }
    for (int sq = square + step; ; sq += step) {
        if (pieces[sq] == OFFBOARD || sq == to) return false;
        if (pieces[sq] == EMPTY) continue;
        bool straight = files == 0 || ranks == 0;
        return pieceColours[pieces[sq]] != side && (straight ? IS_ROOK_OR_QUEEN(pieces[sq]) : IS_BISHOP_OR_QUEEN(pieces[sq]));
    }
}

/// What 'side' can win on the square by taking there, or 0 if it does better not to. 'defender' is the side that
/// didn't make the move being judged: its pinned pieces stay where they are
static Range exchange(int *pieces, int to, int side, int defender) {
    auto canTake = [&](int square) {
        int piece = pieces[square];
        return piece >= wP && piece <= bK && pieceColours[piece] == side && attacks(pieces, square, to)
               && !(side == defender && pinned(pieces, square, side, to));
    };
    int least = INT_MAX;
    for (int square = A1; square <= H8; ++square) {
        if (canTake(square)) {
            least = std::min(least, pieceValues[pieces[square]]);
        }
    }
    if (least == INT_MAX) {
        return {0, 0};
    }
    Range best = {INT_MAX, INT_MIN};
    for (int square = A1; square <= H8; ++square) {
        int piece = pieces[square];
        if (canTake(square) && pieceValues[piece] == least) {
            int taken = pieces[to];
            pieces[to] = piece;
            pieces[square] = EMPTY;
            Range reply = exchange(pieces, to, side ^ 1, defender);
            pieces[square] = piece;
            pieces[to] = taken;
            best.low = std::min(best.low, std::max(0, pieceValues[taken] - reply.high));
            best.high = std::max(best.high, std::max(0, pieceValues[taken] - reply.low));
        }
    }
    return best;
}

static Range exchangeAfter(const Board& board, int move, bool seePins) {
    int pieces[BRD_SQ_NUM];
    std::copy(board.pieces, board.pieces + BRD_SQ_NUM, pieces);
    int from = GET_FROM(move), to = GET_TO(move), promoted = GET_PROMOTED(move);
    int wins = (move & MFLAG_EN_PASSANT) ? pieceValues[wP] : pieceValues[GET_CAPTURED(move)];
    if (promoted != EMPTY) wins += pieceValues[promoted] - pieceValues[wP];
    if (move & MFLAG_EN_PASSANT) pieces[board.side == WHITE ? to - 10 : to + 10] = EMPTY;
    pieces[to] = promoted != EMPTY ? promoted : pieces[from];
    pieces[from] = EMPTY;
    Range reply = exchange(pieces, to, board.side ^ 1, seePins ? board.side ^ 1 : BOTH); // no defender, no pins
    return {wins - reply.high, wins - reply.low};
}

// seeAtLeast only says yes or no. Its value is the highest threshold it says yes to (or 'below FLOOR')
constexpr int FLOOR = -3000, CEILING = 3000;
static int seeValue(const Board& board, int move, bool seePins) {
    if (!seeAtLeast(board, move, FLOOR, seePins)) return FLOOR - 1;
    int yes = FLOOR, no = CEILING + 1;
    while (no - yes > 1) {
        int middle = (yes + no) / 2;
        if (seeAtLeast(board, move, middle, seePins)) yes = middle; else no = middle;
    }
    return yes;
}

// every move here, capture or not, is compared with the second opinion, for the side to move & then with the colours swapped
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
    // pins all over: defenders pinned on files, ranks & diagonals, some of them by the piece that takes
    "4k3/4n3/2n2b2/3p4/B2NP2Q/8/4R3/3RK3 w - - 0 1",
    "r3k2r/1b2bppp/p1nppn2/qp4B1/3NP3/1BN2Q2/PPP2PPP/2KRR3 w kq - 0 1",
    "2r1k2r/pp1bbppp/2n1pn2/q2p2B1/3P4/2NBPN2/PPQ2PPP/R3K2R b KQk - 0 1",
    "3rk3/3q4/2n1b3/1B1p2B1/4P3/2N5/3R4/3QK3 w - - 0 1",
    "6k1/5ppp/8/2b5/8/5N2/r3RPPP/4R1K1 b - - 0 1",
};

static int compareAll(const Board& board, const char *fen) {
    MoveList list;
    generateAllMoves(board, list); // quiet moves too: the search asks whether a piece can be taken where it lands
    for (int i = 0; i < list.count; ++i) {
        int move = list.moves[i].move;
        for (bool seePins : { false, true }) {
            Range slow = exchangeAfter(board, move, seePins);
            int quick = seeValue(board, move, seePins);
            if (quick < std::max(slow.low, FLOOR - 1) || quick > std::max(slow.high, FLOOR - 1)) {
                failures++;
                std::printf("FAIL %s gives %d, the long way gives %d to %d (pins %s): %s\n", printMove(move), quick, slow.low,
                            slow.high, seePins ? "seen" : "not seen", fen);
            }
        }
    }
    return list.count;
}

/// One capture whose value is known: yes at that threshold, no one higher, & the second opinion had better agree
static void checkValue(const char *name, const char *fen, const char *moveString, int value, bool seePins) {
    Board board{};
    if (parseFen(fen, board) != 0) { failures++; std::printf("FAIL bad fen %s\n", fen); return; }
    int move = parseMove(moveString, board);
    if (move == NO_MOVE) { failures++; std::printf("FAIL bad move %s in %s\n", moveString, fen); return; }
    const char *how = seePins ? "seeing pins" : "not seeing pins";
    if (!seeAtLeast(board, move, value, seePins) || seeAtLeast(board, move, value + 1, seePins)) {
        failures++;
        std::printf("FAIL %s, %s: %s should be worth %d, but is worth %d\n", name, how, moveString, value, seeValue(board, move, seePins));
    }
    Range slow = exchangeAfter(board, move, seePins);
    if (slow.low != value || slow.high != value) {
        failures++;
        std::printf("FAIL %s, %s: the long way gives %d to %d, not %d\n", name, how, slow.low, slow.high, value);
    }
}

int main() {
    initAll();

    for (const auto& c : cases) { // no pins in these, so seeing them changes nothing
        checkValue(c.name, c.fen, c.move, c.value, false);
        checkValue(c.name, c.fen, c.move, c.value, true);
    }
    for (const auto& c : pinCases) {
        checkValue(c.name, c.fen, c.move, c.value, true);
        checkValue(c.name, c.fen, c.move, c.blind, false);
    }

    // a king can't take a defended piece at all, so no threshold is low enough
    Board board{};
    parseFen("4k3/8/8/8/8/3p4/4p3/4K3 w - - 0 1", board);
    if (seeAtLeast(board, parseMove("e1e2", board), FLOOR, false) || seeAtLeast(board, parseMove("e1e2", board), FLOOR, true)) {
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

    std::printf("%d moves compared, with & without pins seen, %d failure(s)\n", compared, failures);
    return failures ? 1 : 0;
}
