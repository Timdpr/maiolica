#include "defs.h"
#include <algorithm>

const int knightDirection[8] = { -8, -19, -21, -12, 8, 19, 21, 12 };
const int bishopDirection[4] = { -9, -11, 11, 9 };
const int rookDirection[4]   = { -1, -10, 1, 10 };
const int kingDirection[8]   = { -1, -10, 1, 10, -9, -11, 11, 9 };

/// Call this to determine if the given square is being attacked *by the given side*
int isSquareAttacked(const int square, const int attackingSide, const Board& board) {

    ASSERT(squareOnBoard(square))
    ASSERT(sideValid(attackingSide))
    ASSERT(checkBoard(board))

    // Is it being attacked by a pawn?
    if (attackingSide == WHITE) {
        if (board.pieces[square - 11] == wP || board.pieces[square - 9] == wP) {
            return true;
        }
    } else if (board.pieces[square + 11] == bP || board.pieces[square + 9] == bP) {
        return true;
    }

    // How about a knight?
    for (int nDirection : knightDirection) { // go through 'knightDirection'
        int piece = board.pieces[square + nDirection];
        if (piece != OFFBOARD && IS_KNIGHT(piece) && pieceColours[piece] == attackingSide) {
            return true;
        }
    }

    // What about rooks and queens moving in cardinal directions?
    for (int rDirection : rookDirection) { // for each direction
        int tempSquare = square + rDirection;
        int piece = board.pieces[tempSquare];
        while (piece != OFFBOARD) {
            if (piece != EMPTY) {
                if (IS_ROOK_OR_QUEEN(piece) && pieceColours[piece] == attackingSide) {
                    return true;
                }
                break; // we have hit a piece, any piece, so break out of loop
            } // if no piece found, continue 'sliding' in the current direction:
            tempSquare += rDirection;
            piece = board.pieces[tempSquare];
        }
    }

    // What about bishops and queens moving diagonally?
    for (int bDirection : bishopDirection) { // for each direction
        int tempSquare = square + bDirection;
        int piece = board.pieces[tempSquare];
        while (piece != OFFBOARD) {
            if (piece != EMPTY) {
                if (IS_BISHOP_OR_QUEEN(piece) && pieceColours[piece] == attackingSide) {
                    return true;
                }
                break; // we have hit a piece, any piece, so break out of loop
            } // if no piece found, continue 'sliding' in the current direction:
            tempSquare += bDirection;
            piece = board.pieces[tempSquare];
        }
    }

    // And finally, how about the kings?
    for (int kDirection : kingDirection) { // go through 'knightDirection'
        int piece = board.pieces[square + kDirection];
        if (piece != OFFBOARD && IS_KING(piece) && pieceColours[piece] == attackingSide) {
            return true;
        }
    }

    return false; // If no attacking piece can be found, return false!
}

/// The first piece met going out from a square along a line, or OFFBOARD. 'square' ends up where it stands.
/// Up to two squares are looked through as if empty: their pieces have already left
static inline int nextOnLine(const Board& board, int& square, const int step, const int gone, const int alsoGone) {
    int piece;
    do {
        square += step;
        piece = board.pieces[square];
    } while (piece == EMPTY || (piece != OFFBOARD && (square == gone || square == alsoGone)));
    return piece;
}

/// Is the piece on 'square' pinned to its own king, in an exchange on 'to'? That is: it stands on a line with its king,
/// nothing between them, & the next piece along that line beyond it is an enemy bishop, rook or queen that moves that
/// way. The squares in 'gone' are looked through: their pieces have left. 'to' itself stops the line whichever side of
/// the piece it lies, so a piece that takes along the very line it is pinned on isn't called pinned
static inline int pinnedToKing(const Board& board, const int square, const int colour, const int to, const int *gone, const int goneCount) {
    const int king = board.kingSq[colour];
    const int step = lineStep[square - king + 77]; // from the king towards the piece
    if (step == 0) {
        return false; // not on a line with the king at all (or it is the king)
    }
    auto left = [&](int sq) { return std::find(gone, gone + goneCount, sq) != gone + goneCount; };
    // from the king to the piece: is the way clear? (a few far-apart pairs get a step without sharing a line: for those
    // this walk runs off the board, which isn't EMPTY, before it arrives)
    for (int sq = king + step; sq != square; sq += step) {
        if (sq == to || (board.pieces[sq] != EMPTY && !left(sq))) {
            return false;
        }
    }
    for (int sq = square + step; ; sq += step) { // & beyond the piece: what comes first?
        const int piece = board.pieces[sq];
        if (piece == OFFBOARD || sq == to) {
            return false;
        }
        if (piece != EMPTY && !left(sq)) {
            const int straight = step == 1 || step == -1 || step == 10 || step == -10;
            return pieceColours[piece] != colour && (straight ? IS_ROOK_OR_QUEEN(piece) : IS_BISHOP_OR_QUEEN(piece));
        }
    }
}

/// Static exchange evaluation, or 'SEE'. Say this move is made, and the two sides then take turns capturing on its
/// square, each with its least valuable piece, either free to stop when taking again would cost it. Does the side that
/// made the move end up at least 'threshold' ahead, in pieceValues? Nothing is searched & no move is made.
/// A piece lined up behind another joins in once the one in front has gone. With 'seePins', a piece of the other side's
/// that is pinned to its king can't take back (the mover's own pinned pieces still can: docs/architecture.md says why).
/// That costs about a sixth more, so it is asked for where a wrong 'no' is dearest
int seeAtLeast(const Board& board, const int move, const int threshold, const int seePins) {
    const int from = GET_FROM(move);
    const int to = GET_TO(move);
    const int promoted = GET_PROMOTED(move);
    const int side = board.side;

    // the best case: what the move wins if nothing takes back
    int balance = ((move & MFLAG_EN_PASSANT) ? pieceValues[wP] : pieceValues[GET_CAPTURED(move)]) - threshold;
    if (promoted != EMPTY) {
        balance += pieceValues[promoted] - pieceValues[wP];
    }
    if (balance < 0) {
        return false;
    }
    // the worst case: the piece that lands on the square is lost for nothing
    balance -= pieceValues[promoted != EMPTY ? promoted : board.pieces[from]];
    if (balance >= 0) {
        return true;
    }

    // Squares whose pieces have left: the mover's, a pawn taken en passant, & later each piece used in the exchange
    const int passed = (move & MFLAG_EN_PASSANT) ? (side == WHITE ? to - 10 : to + 10) : from;
    int gone[32];
    int goneCount = 0;
    gone[goneCount++] = from;
    gone[goneCount++] = passed;

    // A pawn takes back first if one can (& isn't pinned), & when the piece is worth this much more than what it
    // took, that settles it
    if (balance + pieceValues[wP] < 0) {
        const int pawn = (side == WHITE) ? bP : wP;
        for (int square : { side == WHITE ? to + 9 : to - 9, side == WHITE ? to + 11 : to - 11 }) {
            if (board.pieces[square] == pawn && !(seePins && pinnedToKing(board, square, side ^ 1, to, gone, goneCount))) {
                return false;
            }
        }
    }

    // Otherwise, find everything that attacks the square, for both sides: where each piece is & the line it is on
    // (0 for a knight). The moving piece has left, as has a pawn taken en passant, so what's behind them counts
    int squares[2][16], steps[2][16];
    int count[2] = { 0, 0 };

    for (int i = 0; i < 8; ++i) { // kingDirection: the four rook lines, then the four bishop lines
        int step = kingDirection[i];
        int square = to;
        int piece = nextOnLine(board, square, step, from, passed);
        if (piece == OFFBOARD) {
            continue;
        }
        int attacks = i < 4 ? IS_ROOK_OR_QUEEN(piece) : IS_BISHOP_OR_QUEEN(piece);
        if (!attacks && square == to + step) { // right next to it: a king, or a pawn (which takes diagonally forwards)
            attacks = IS_KING(piece) || (piece == wP && (step == -9 || step == -11)) || (piece == bP && (step == 9 || step == 11));
        }
        if (attacks) {
            int colour = pieceColours[piece];
            squares[colour][count[colour]] = square;
            steps[colour][count[colour]++] = step;
        }
    }
    for (int nDirection : knightDirection) {
        int piece = board.pieces[to + nDirection];
        if (piece != OFFBOARD && IS_KNIGHT(piece) && to + nDirection != from) {
            int colour = pieceColours[piece];
            squares[colour][count[colour]] = to + nDirection;
            steps[colour][count[colour]++] = 0;
        }
    }

    // Now the two sides take turns. Whoever is to take needs to: 'balance' says they are behind as things stand
    int colour = side ^ 1;
    int kingUsed = false; // once the other side's king has taken, its pins mean nothing
    while (count[colour] > 0) {
        // take with the least valuable piece
        int best = 0;
        for (int i = 1; i < count[colour]; ++i) {
            if (pieceValues[board.pieces[squares[colour][i]]] < pieceValues[board.pieces[squares[colour][best]]]) {
                best = i;
            }
        }
        // ...though the side that didn't make the move must pass over a piece that is pinned to its king
        if (seePins && colour != side && !kingUsed) {
            unsigned pinned = 0; // a bit for each piece found pinned, this turn
            while (best >= 0 && pinnedToKing(board, squares[colour][best], colour, to, gone, goneCount)) {
                pinned |= 1u << best;
                best = -1;
                for (int i = 0; i < count[colour]; ++i) {
                    if (!(pinned >> i & 1)
                        && (best < 0 || pieceValues[board.pieces[squares[colour][i]]] < pieceValues[board.pieces[squares[colour][best]]])) {
                        best = i;
                    }
                }
            }
            if (best < 0) {
                break; // every piece of theirs that could take is pinned
            }
        }
        int square = squares[colour][best];
        int step = steps[colour][best];
        int value = pieceValues[board.pieces[square]];
        gone[goneCount++] = square;
        kingUsed |= colour != side && IS_KING(board.pieces[square]);
        squares[colour][best] = squares[colour][--count[colour]];
        steps[colour][best] = steps[colour][count[colour]];

        // it has left its line, so a rook, bishop or queen behind it (either side's) now attacks the square too
        if (step != 0) {
            int piece = nextOnLine(board, square, step, NO_SQ, NO_SQ);
            if (piece != OFFBOARD && ((step == 1 || step == -1 || step == 10 || step == -10) ? IS_ROOK_OR_QUEEN(piece) : IS_BISHOP_OR_QUEEN(piece))) {
                int behind = pieceColours[piece];
                squares[behind][count[behind]] = square;
                steps[behind][count[behind]++] = step;
            }
        }

        // the other side's turn. Is the side that just took ahead even if this piece is lost in its turn? Then it's over.
        // (a king is worth so much that it never is: so a king only 'takes' when nothing can take it back)
        colour ^= 1;
        balance = -balance - 1 - value;
        if (balance >= 0) {
            break;
        }
    }
    return colour != side; // whoever was left needing to take, & couldn't or didn't, comes out behind
}
