#include "defs.h"

#define HASH_PIECE(piece, square) (board->positionKey ^= (pieceKeys[(piece)][(square)]))
#define HASH_CASTLE (board->positionKey ^= (castleKeys[(board->castlingPerms)]))
#define HASH_SIDE (board->positionKey ^= (sideKey))
#define HASH_EN_PASSANT (board->positionKey ^= (pieceKeys[EMPTY][(board->enPasSq)]))

/// castling permission &= castlePermArray[from] (remember 15 == 1111, then if you &= 3 it will equal 0011)
const int castlePermArray[120] = {
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 13, 15, 15, 15, 12, 15, 15, 14, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15,  7, 15, 15, 15,  3, 15, 15, 11, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15
};

/// Clear the piece on the given square from the given board (& update other relevant fields)
static void clearPiece(const int square, Board *board) {
    ASSERT(squareOnBoard(square))
    int piece = board->pieces[square]; // get piece from board's pieces array
    ASSERT(pieceValid(piece))

    int colour = pieceColours[piece];

    HASH_PIECE(piece, square); // hash piece out of position key

    board->pieces[square] = EMPTY; // set piece in board's pieces array to empty
    board->material[colour] -= pieceValues[piece]; // take piece value away from side's material score

    if (bigPieces[piece]) { // if piece is not a pawn
        board->bigPieces[colour]--; // decrement bigPieces count for side
        if (majorPieces[piece]) { // if major/minor, decrement major/minor
            board->majorPieces[colour]--;
        } else {
            board->minorPieces[colour]--;
        }
    } else { // if piece is a pawn, then remove from the pawn bitboards
        CLEAR_BIT(board->pawns[colour], INDEX_120_TO_64(square));
        CLEAR_BIT(board->pawns[BOTH], INDEX_120_TO_64(square));
    }

    // Now remove piece from piece list...
    int removedPieceNumber = -1;
    for (int i = 0; i < board->pieceCounts[piece]; ++i) { // (for as many pieces of this type exist)
        if (board->pieceList[piece][i] == square) { // loop through all of this type of piece, and if the square number matches...
            removedPieceNumber = i; // save the piece number
            break;
        }
    }
    ASSERT(removedPieceNumber != -1)
    board->pieceCounts[piece]--; // decrement count of this type of piece
    // Tricky! Move the last piece in the array to the place where the removed piece was:
    board->pieceList[piece][removedPieceNumber] = board->pieceList[piece][board->pieceCounts[piece]];
    /* e.g.:
     *   [wP][0] = 32
     *   [wP][1] = 45
     * - Remove wP @ square 32 -
     *   [wP][0] = 45
     *   [wP][1] = 45 BUT pieceCounts[wP] is now 1 so this will not be reached
     */
}

/// Add the given piece at the given square on the given board! (& update other relevant fields)
static void addPiece(const int square, Board *board, const int piece) {
    ASSERT(squareOnBoard(square))
    ASSERT(pieceValid(piece))

    int colour = pieceColours[piece];

    HASH_PIECE(piece, square); // hash piece into position key

    board->pieces[square] = piece;

    if (bigPieces[piece]) { // if piece is not a pawn
        board->bigPieces[colour]++; // increment bigPieces count for side
        if (majorPieces[piece]) { // if major/minor, increment major/minor
            board->majorPieces[colour]++;
        } else {
            board->minorPieces[colour]++;
        }
    } else { // if piece is a pawn, then remove from the pawn bitboards
        SET_BIT(board->pawns[colour], INDEX_120_TO_64(square));
        SET_BIT(board->pawns[BOTH], INDEX_120_TO_64(square));
    }
    board->material[colour] += pieceValues[piece]; // update material value
    // Add piece to piece list at the end of the array by using the piece count as the index, then increment the piece count:
    board->pieceList[piece][board->pieceCounts[piece]++] = square;
}

static void movePiece(const int from, const int to, Board *board) {
    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))

    int piece = board->pieces[from];
    int colour = pieceColours[piece];
#ifdef DEBUG
    int removedPieceNumber = FALSE; // Preprocesser: for checking that we find a piece, when in debug mode
#endif

    HASH_PIECE(piece, from);
    board->pieces[from] = EMPTY;

    HASH_PIECE(piece, to);
    board->pieces[to] = piece;

    if (!bigPieces[piece]) { // if piece is a pawn, adjust pawn bitboards
        CLEAR_BIT(board->pawns[colour], INDEX_120_TO_64(from));
        CLEAR_BIT(board->pawns[BOTH], INDEX_120_TO_64(from));
        SET_BIT(board->pawns[colour], INDEX_120_TO_64(to));
        SET_BIT(board->pawns[BOTH], INDEX_120_TO_64(to));
    }

    for (int i = 0; i < board->pieceCounts[piece]; ++i) { // loop through this type of piece in the piece list...
        if (board->pieceList[piece][i] == from) { // ...and if a piece's square number matches our 'from' square...
            board->pieceList[piece][i] = to; // ...then set it to our 'to' square!
#ifdef DEBUG
            removedPieceNumber = TRUE; // set this so we can check if a piece was found when in debug mode
#endif
            break;
        }
    }
    ASSERT(removedPieceNumber)
}

// returns false if the side making the move is in check after the move
// TODO: Can't we get hash from history?
int makeMove(Board *board, int move) { // todo: return bool?
    ASSERT(checkBoard(board))

    int from = GET_FROM(move);
    int to = GET_TO(move);
    int side = board->side;

    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))
    ASSERT(sideValid(side))
    ASSERT(pieceValid(board->pieces[from]))

    // Update history at current ply
    board->history[board->historyPly].positionKey = board->positionKey; // store hashkey

    // If en passant, then remove piece behind moving piece
    if (move & MFLAG_EN_PASSANT) {
        if (side == WHITE) {
            clearPiece(to-10, board);
        } else {
            clearPiece(to+10, board);
        }
    // Move piece if castling:
    } else if (move & MFLAG_CASTLING) {
        switch (to) {
            case C1:
                movePiece(A1, D1, board); break;
            case C8:
                movePiece(A8, D8, board); break;
            case G1:
                movePiece(H1, F1, board); break;
            case G8:
                movePiece(H8, F8, board); break;
            default: ASSERT(FALSE); break;
        }
    }
    // hash en passant square if it was available:
    if (board->enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    HASH_CASTLE; // hash out castling permissions
    // store info in history array:
    board->history[board->historyPly].move = move;
    board->history[board->historyPly].fiftyMove = board->fiftyMove;
    board->history[board->historyPly].enPasSq = board->enPasSq;
    board->history[board->historyPly].castlingPerms = board->castlingPerms;
    // adjust castling permissions:
    board->castlingPerms &= castlePermArray[from];
    board->castlingPerms &= castlePermArray[to];
    board->enPasSq = NO_SQ;
    // hash castling permissions back in
    HASH_CASTLE;

    int captured = GET_CAPTURED(move);
    board->fiftyMove++; // update fifty move counter
    if (captured != EMPTY) { // if a piece was captured
        ASSERT(pieceValid(captured))
        clearPiece(to, board); // clear it!
        board->fiftyMove = 0; // and reset fifty move counter
    }
    // update ply counters
    board->historyPly++;
    board->ply++;

    // do we need to set a new en passant square?
    if (piecePawn[board->pieces[from]]) { // if piece is a pawn
        board->fiftyMove = 0; // reset fifty move counter
        if (move & MFLAG_PAWN_START) { // if move was pawn start
            if (side == WHITE) {
                board->enPasSq = from+10; // set en passant square
                ASSERT(ranksBoard[board->enPasSq] == RANK_3)
            } else {
                board->enPasSq = from-10;
                ASSERT(ranksBoard[board->enPasSq] == RANK_6)
            }
            HASH_EN_PASSANT; // hash in the en passant square now it is set
        }
    }

    // Now the piece can be moved!
    movePiece(from, to, board);

    // Can it be promoted?
    int promotedPiece = GET_PROMOTED(move);
    if (promotedPiece != EMPTY) { // if yes...
        ASSERT(pieceValid(promotedPiece) && !piecePawn[promotedPiece])
        clearPiece(to, board); // ...clear the pawn
        addPiece(to, board, promotedPiece); // and add the promoted piece!
    }

    if (pieceKing[board->pieces[to]]) {
        board->kingSq[board->side] = to; // todo: this may be redundant
    }

    // Switch sides!
    board->side ^= 1;
    HASH_SIDE;

    ASSERT(checkBoard(board));

    // If the king is attacked by the new side to move, take back the move and return false
    if (isSquareAttacked(board->kingSq[side], board->side, board)) {
        takeMove(board);
        return FALSE;
    }

    return TRUE;
}

/// Undo a move
void takeMove(Board *board) {
    ASSERT(checkBoard(board))

    board->historyPly--;
    board->ply--;

    int move = board->history[board->historyPly].move;
    int from = GET_FROM(move);
    int to = GET_TO(move);

    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))

    if (board->enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    HASH_CASTLE;

    board->castlingPerms = board->history[board->historyPly].castlingPerms;
    board->fiftyMove = board->history[board->historyPly].fiftyMove;
    board->enPasSq = board->history[board->historyPly].enPasSq;

    if (board->enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    HASH_CASTLE;

    board->side ^= 1;
    HASH_SIDE;

    if (MFLAG_EN_PASSANT & move) {
        if (board->side == WHITE) {
            addPiece(to-10, board, bP);
        } else {
            addPiece(to+10, board, wP);
        }
    } else if (MFLAG_CASTLING & move) {
        switch (to) {
            case C1: movePiece(D1, A1, board); break;
            case C8: movePiece(D8, A8, board); break;
            case G1: movePiece(F1, H1, board); break;
            case G8: movePiece(F8, H8, board); break;
            default: ASSERT(FALSE); break;
        }
    }

    movePiece(to, from, board); // Note the reversed to/from!

    if (pieceKing[board->pieces[from]]) {
        board->kingSq[board->side] = from;
    }

    int captured = GET_CAPTURED(move); // re-add captured piece if present
    if (captured != EMPTY) {
        ASSERT(pieceValid(captured))
        addPiece(to, board, captured);
    }

    if (GET_PROMOTED(move) != EMPTY) {
        ASSERT(pieceValid(GET_PROMOTED(move)) && !piecePawn[GET_PROMOTED(move)])
        clearPiece(from, board); // get rid of promoted-to piece
        addPiece(from, board, (pieceColours[GET_PROMOTED(move)] == WHITE ? wP : bP)); // re-add pawn of correct colour
    }

    ASSERT(checkBoard(board))
}
