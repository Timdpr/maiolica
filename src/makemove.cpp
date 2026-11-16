#include "defs.h"

#define HASH_PIECE(piece, square) (board.positionKey ^= (pieceKeys[(piece)][(square)]))
#define HASH_CASTLE (board.positionKey ^= (castleKeys[(board.castlingPerms)]))
#define HASH_SIDE (board.positionKey ^= (sideKey))
#define HASH_EN_PASSANT (board.positionKey ^= (pieceKeys[EMPTY][(board.enPasSq)]))

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

/// The step that takes you from one square towards another on the same rank, file or diagonal,
/// indexed by the difference between the squares + 77. 0 if they don't share a line
static int lineStep[155];

void initLineSteps() {
    for (int step : { 1, 9, 10, 11 }) {
        for (int distance = 1; distance <= 7; ++distance) {
            lineStep[77 + step * distance] = step;
            lineStep[77 - step * distance] = -step;
        }
    }
    // the board is 10 wide, so a few far-apart pairs that aren't on a line get a step too
    // (d1 & g7 differ by 63, the same as h1 & a8). Following one just finds nothing
}

/// Is the side to move in check? Worked out the first time it's asked of a position, then remembered
int isInCheck(Board& board) {
    int& inCheck = board.history[board.historyPly].inCheck;
    if (inCheck == CHECK_UNKNOWN) {
        inCheck = isSquareAttacked(board.kingSq[board.side], board.side^1, board);
    }
    ASSERT(inCheck == isSquareAttacked(board.kingSq[board.side], board.side^1, board))
    return inCheck;
}

/// Has a piece leaving the 'from' square opened a line to the given side's king?
/// The whole legality test for a move, as long as the king wasn't in check before it,
/// didn't move itself, and the move wasn't en passant (which empties two squares)
static int kingExposed(const int from, const int side, const Board& board) {
    int kingSquare = board.kingSq[side];
    int step = lineStep[from - kingSquare + 77];
    if (step == 0) { // not on a line with the king, so nothing was opened
        return false;
    }
    // look along the line from the king until we hit something
    int square = kingSquare + step;
    while (board.pieces[square] == EMPTY) {
        square += step;
    }
    int piece = board.pieces[square];
    if (piece == OFFBOARD || pieceColours[piece] == side) {
        return false;
    }
    if (step == 1 || step == -1 || step == 10 || step == -10) {
        return IS_ROOK_OR_QUEEN(piece);
    }
    return IS_BISHOP_OR_QUEEN(piece);
}

/// Clear the piece on the given square from the given board (& update other relevant fields)
static void clearPiece(const int square, Board& board) {
    ASSERT(squareOnBoard(square))
    int piece = board.pieces[square]; // get piece from board's pieces array
    ASSERT(pieceValid(piece))

    int colour = pieceColours[piece];

    HASH_PIECE(piece, square); // hash piece out of position key

    board.pieces[square] = EMPTY; // set piece in board's pieces array to empty
    board.material[colour] -= pieceValues[piece]; // take piece value away from side's material score

    if (bigPieces[piece]) { // if piece is not a pawn
        board.bigPieces[colour]--; // decrement bigPieces count for side
        if (majorPieces[piece]) { // if major/minor, decrement major/minor
            board.majorPieces[colour]--;
        } else {
            board.minorPieces[colour]--;
        }
    } else { // if piece is a pawn, then remove from the pawn bitboards
        CLEAR_BIT(board.pawns[colour], INDEX_120_TO_64(square));
        CLEAR_BIT(board.pawns[BOTH], INDEX_120_TO_64(square));
    }

    // Now remove piece from piece list...
    int removedPieceNumber = board.pieceIndex[square]; // no need to search, we know where it is
    ASSERT(board.pieceList[piece][removedPieceNumber] == square)
    board.pieceCounts[piece]--; // decrement count of this type of piece
    // Tricky! Move the last piece in the array to the place where the removed piece was:
    int lastSquare = board.pieceList[piece][board.pieceCounts[piece]];
    board.pieceList[piece][removedPieceNumber] = lastSquare;
    board.pieceIndex[lastSquare] = removedPieceNumber; // ...and remember its new place
    /* e.g.:
     *   [wP][0] = 32
     *   [wP][1] = 45
     * - Remove wP @ square 32 -
     *   [wP][0] = 45
     *   [wP][1] = 45 BUT pieceCounts[wP] is now 1 so this will not be reached
     */
}

/// Add the given piece at the given square on the given board! (& update other relevant fields)
static void addPiece(const int square, Board& board, const int piece) {
    ASSERT(squareOnBoard(square))
    ASSERT(pieceValid(piece))

    int colour = pieceColours[piece];

    HASH_PIECE(piece, square); // hash piece into position key

    board.pieces[square] = piece;

    if (bigPieces[piece]) { // if piece is not a pawn
        board.bigPieces[colour]++; // increment bigPieces count for side
        if (majorPieces[piece]) { // if major/minor, increment major/minor
            board.majorPieces[colour]++;
        } else {
            board.minorPieces[colour]++;
        }
    } else { // if piece is a pawn, then remove from the pawn bitboards
        SET_BIT(board.pawns[colour], INDEX_120_TO_64(square));
        SET_BIT(board.pawns[BOTH], INDEX_120_TO_64(square));
    }
    board.material[colour] += pieceValues[piece]; // update material value
    // Add piece to piece list at the end of the array by using the piece count as the index, then increment the piece count:
    board.pieceIndex[square] = board.pieceCounts[piece];
    board.pieceList[piece][board.pieceCounts[piece]++] = square;
}

static void movePiece(const int from, const int to, Board& board) {
    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))

    int piece = board.pieces[from];
    int colour = pieceColours[piece];

    HASH_PIECE(piece, from);
    board.pieces[from] = EMPTY;

    HASH_PIECE(piece, to);
    board.pieces[to] = piece;

    if (!bigPieces[piece]) { // if piece is a pawn, adjust pawn bitboards
        CLEAR_BIT(board.pawns[colour], INDEX_120_TO_64(from));
        CLEAR_BIT(board.pawns[BOTH], INDEX_120_TO_64(from));
        SET_BIT(board.pawns[colour], INDEX_120_TO_64(to));
        SET_BIT(board.pawns[BOTH], INDEX_120_TO_64(to));
    }

    int pieceNumber = board.pieceIndex[from]; // the piece keeps its place in the piece list...
    ASSERT(board.pieceList[piece][pieceNumber] == from)
    board.pieceList[piece][pieceNumber] = to; // ...and just gets our 'to' square!
    board.pieceIndex[to] = pieceNumber;
}

// returns false if the side making the move is in check after the move
// TODO: Can't we get hash from history?
int makeMove(Board& board, int move) {
    ASSERT(checkBoard(board))

    int from = GET_FROM(move);
    int to = GET_TO(move);
    int side = board.side;

    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))
    ASSERT(sideValid(side))
    ASSERT(pieceValid(board.pieces[from]))

    // Most moves can only leave our king in check by uncovering it. The others get the full test at the end:
    // king moves (castling too), en passant, and anything played while in check. Ask before the board changes!
    int fullTest = IS_KING(board.pieces[from]) || (move & MFLAG_EN_PASSANT) || isInCheck(board);

    // Update history at current ply
    board.history[board.historyPly].positionKey = board.positionKey; // store hashkey

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
            default: ASSERT(false) break;
        }
    }
    // hash en passant square if it was available:
    if (board.enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    HASH_CASTLE; // hash out castling permissions
    // store info in history array:
    board.history[board.historyPly].move = move;
    board.history[board.historyPly].fiftyMove = board.fiftyMove;
    board.history[board.historyPly].enPasSq = board.enPasSq;
    board.history[board.historyPly].castlingPerms = board.castlingPerms;
    // adjust castling permissions:
    board.castlingPerms &= castlePermArray[from];
    board.castlingPerms &= castlePermArray[to];
    board.enPasSq = NO_SQ;
    // hash castling permissions back in
    HASH_CASTLE;

    int captured = GET_CAPTURED(move);
    board.fiftyMove++; // update fifty move counter
    if (captured != EMPTY) { // if a piece was captured
        ASSERT(pieceValid(captured))
        clearPiece(to, board); // clear it!
        board.fiftyMove = 0; // and reset fifty move counter
    }
    // update ply counters
    board.historyPly++;
    board.ply++;
    board.history[board.historyPly].inCheck = CHECK_UNKNOWN; // a new position, so nobody has asked yet

    // do we need to set a new en passant square?
    if (piecePawn[board.pieces[from]]) { // if piece is a pawn
        board.fiftyMove = 0; // reset fifty move counter
        if (move & MFLAG_PAWN_START) { // if move was pawn start
            if (side == WHITE) {
                board.enPasSq = from+10; // set en passant square
                ASSERT(ranksBoard[board.enPasSq] == RANK_3)
            } else {
                board.enPasSq = from-10;
                ASSERT(ranksBoard[board.enPasSq] == RANK_6)
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

    if (pieceKing[board.pieces[to]]) {
        board.kingSq[board.side] = to; // todo: this may be redundant
    }

    // Switch sides!
    board.side ^= 1;
    HASH_SIDE;

    ASSERT(checkBoard(board))

    // If the king is attacked by the new side to move, take back the move and return false
    // (taking back, rather than never making it, as a capture made & undone reorders the piece lists)
    int kingAttacked = fullTest ? isSquareAttacked(board.kingSq[side], board.side, board) : kingExposed(from, side, board);
    ASSERT(kingAttacked == isSquareAttacked(board.kingSq[side], board.side, board))
    if (kingAttacked) {
        undoMove(board);
        return false;
    }

    return true;
}

/// Undo a move
void undoMove(Board& board) {
    ASSERT(checkBoard(board))

    board.historyPly--;
    board.ply--;

    int move = board.history[board.historyPly].move;
    int from = GET_FROM(move);
    int to = GET_TO(move);

    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))

    if (board.enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    HASH_CASTLE;

    board.castlingPerms = board.history[board.historyPly].castlingPerms;
    board.fiftyMove = board.history[board.historyPly].fiftyMove;
    board.enPasSq = board.history[board.historyPly].enPasSq;

    if (board.enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    HASH_CASTLE;

    board.side ^= 1;
    HASH_SIDE;

    if (MFLAG_EN_PASSANT & move) {
        if (board.side == WHITE) {
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
            default: ASSERT(false); break;
        }
    }

    movePiece(to, from, board); // Note the reversed to/from!

    if (pieceKing[board.pieces[from]]) {
        board.kingSq[board.side] = from;
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

/// Update and switch side as usual, but no move is made
void makeNullMove(Board& board) {
    ASSERT(checkBoard(board))
    ASSERT(!isSquareAttacked(board.kingSq[board.side], board.side^1, board))

    board.ply++;
    board.history[board.historyPly].positionKey = board.positionKey;

    if (board.enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    board.history[board.historyPly].move = NO_MOVE;
    board.history[board.historyPly].fiftyMove = board.fiftyMove; // todo: should this be incremented here (then taken back)? Probably!
    board.history[board.historyPly].enPasSq = board.enPasSq;
    board.history[board.historyPly].castlingPerms = board.castlingPerms;
    board.enPasSq = NO_SQ;

    board.side ^= 1;
    board.historyPly++;
    board.history[board.historyPly].inCheck = CHECK_UNKNOWN;
    HASH_SIDE;

    ASSERT(checkBoard(board))
}

/// Take back a null move!
void undoNullMove(Board& board) {
    ASSERT(checkBoard(board))

    board.historyPly--;
    board.ply--;

    if (board.enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    board.castlingPerms = board.history[board.historyPly].castlingPerms;
    board.fiftyMove = board.history[board.historyPly].fiftyMove;
    board.enPasSq = board.history[board.historyPly].enPasSq;

    if (board.enPasSq != NO_SQ) {
        HASH_EN_PASSANT;
    }
    board.side ^= 1;
    HASH_SIDE;

    ASSERT(checkBoard(board))
}
