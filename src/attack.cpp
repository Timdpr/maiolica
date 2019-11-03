
#include "defs.h"

const int knightDirection[8] = { -8, -19, -21, -12, 8, 19, 21, 12 };
const int bishopDirection[4] = { -9, -11, 11, 9 };
const int rookDirection[4]   = { -1, -10, 1, 10 };
const int kingDirection[8]   = { -1, -10, 1, 10, -9, -11, 11, 9 };

/// Call this to determine if the given square is being attacked *by the given side*
int isSquareAttacked(const int square, const int attackingSide, const Board *board) {

    // Is it being attacked by a pawn?
    if (attackingSide == WHITE) {
        if (board->pieces[square - 11] == wP || board->pieces[square - 9] == wP) {
            return TRUE;
        }
    } else if (board->pieces[square + 11] == bP || board->pieces[square + 9] == bP) {
        return TRUE;
    }

    // How about a knight?
    for (int i = 0; i < 8; ++i) { // go through 'knightDirection'
        int piece = board->pieces[square + knightDirection[i]];
        if (IS_KNIGHT(piece) && piecesColour[piece] == attackingSide) {
            return TRUE;
        }
    }

    // What about rooks and queens moving in cardinal directions?
    for (int i = 0; i < 4; ++i) { // for each direction
        int direction = rookDirection[i];
        int tempSquare = square + direction;
        int piece = board->pieces[tempSquare];
        while (piece != OFFBOARD) {
            if (piece != EMPTY) {
                if (IS_ROOK_OR_QUEEN(piece) && piecesColour[piece] == attackingSide) {
                    return TRUE;
                }
                break; // we have hit a piece, any piece, so break out of loop
            } // if no piece found, continue 'sliding' in the current direction:
            tempSquare += direction;
            piece = board->pieces[tempSquare];
        }
    }

    // What about bishops and queens moving diagonally?
    for (int i = 0; i < 4; ++i) { // for each direction
        int direction = bishopDirection[i];
        int tempSquare = square + direction;
        int piece = board->pieces[tempSquare];
        while (piece != OFFBOARD) {
            if (piece != EMPTY) {
                if (IS_BISHOP_OR_QUEEN(piece) && piecesColour[piece] == attackingSide) {
                    return TRUE;
                }
                break; // we have hit a piece, any piece, so break out of loop
            } // if no piece found, continue 'sliding' in the current direction:
            tempSquare += direction;
            piece = board->pieces[tempSquare];
        }
    }

    // And finally, how about the kings?
    for (int i = 0; i < 8; ++i) { // go through 'knightDirection'
        int piece = board->pieces[square + kingDirection[i]];
        if (IS_KING(piece) && piecesColour[piece] == attackingSide) {
            return TRUE;
        }
    }

    return FALSE; // If no attacking piece can be found, return false!
}