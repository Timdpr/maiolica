#include "defs.h"

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