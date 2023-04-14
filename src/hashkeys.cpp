#include "defs.h"

///
U64 generatePositionKey(const Board& board) { // returns rather than sets key

    U64 finalKey = 0;

    // Loop through squares on board
    for (int square = 0; square < BRD_SQ_NUM; ++square) {
        int piece = board.pieces[square]; // set piece to whatever is in board's pieces array at this square
        if (piece != NO_SQ && piece != EMPTY && piece != OFFBOARD) { // if piece is not a border square AND not empty...
            ASSERT(piece >= wP && piece <= bK) // ...then it must be a real piece (white pawn up to black king)
            finalKey ^= pieceKeys[piece][square]; // hash in whatever number is stored at piece keys array here
        }
    }

    // If white to move, hash in the side key
    if (board.side == WHITE) {
        finalKey ^= sideKey;
    }

    // en passant: if pawn has moved 2 squares in prev. move, en passant square is set
    // If en passant square is set, and within bounds, then hash in
    if (board.enPasSq != NO_SQ) {
        ASSERT(board.enPasSq >= 0 && board.enPasSq < BRD_SQ_NUM)
        finalKey ^= pieceKeys[EMPTY][board.enPasSq];
    }

    // Hash in castling permission key
    ASSERT(board.castlingPerms >= 0 && board.castlingPerms <= 15)
    finalKey ^= castleKeys[board.castlingPerms];

    return finalKey;
}
