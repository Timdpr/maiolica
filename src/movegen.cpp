#include "defs.h"

#define MOVE(from, to, captured, promoted, flag) ( (from) | ((to) << 7) | ((captured) << 14) | ((promoted) << 20 ) | (flag) )
#define SQUARE_OFFBOARD(square) (filesBoard[(square)] == OFFBOARD)

const int victimScore[13] = {0, 100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600};
static int mVVLVAScores[13][13];

/*
 * Move ordering:
 * Search for...
 * - PV Move
 * - Captures -> MvvLVA (most valuable victim, least valuable attacker) (so P takes Q descending to Q takes P)
 * - Killers (non-capturing)
 * - HistoryScore (non-capturing)
 */

/// Use this array to loop through slide pieces for a side. Start at index given by slideLoopStartIndex[SIDE], and stop at '0'.
const int slidePiecesArray[8]       = { wB, wR, wQ, 0, bB, bR, bQ, 0 };
const int slideLoopStartIndex[2]    = { 0,          4 };
/// Use this array to loop through non-slide pieces for a side. Start at index given by nonSlideLoopStartIndex[SIDE], and stop at '0'.
const int nonSlidePiecesArray[6]    = { wN, wK, 0, bN, bK, 0 };
const int nonSlideLoopStartIndex[2] = { 0,      3 };
/// [piece][direction]: Possible directions per piece (adding direction number to square index takes you in that direction)
const int pieceDirection [13][8] = {
    {  0,  0,  0,  0,  0,  0,  0,  0 },
    {  0,  0,  0,  0,  0,  0,  0,  0 },
    { -8,-19,-21,-12,  8, 19, 21, 12 },
    { -9,-11, 11,  9,  0,  0,  0,  0 },
    { -1,-10,  1, 10,  0,  0,  0,  0 },
    { -1,-10,  1, 10, -9,-11, 11,  9 },
    { -1,-10,  1, 10, -9,-11, 11,  9 },
    {  0,  0,  0,  0,  0,  0,  0,  0 },
    { -8,-19,-21,-12,  8, 19, 21, 12 },
    { -9,-11, 11,  9,  0,  0,  0,  0 },
    { -1,-10,  1, 10,  0,  0,  0,  0 },
    { -1,-10,  1, 10, -9,-11, 11,  9 },
    { -1,-10,  1, 10, -9,-11, 11,  9 }
};
/// Tells you how many directions there are in the rows of the pieceDirection array (indexed by piece)
const int pieceDirectionNumbers[13] = { 0, 0, 8, 4, 4, 8, 8, 0, 8, 4, 4, 8, 8 };

/// Adds a 'quiet' (non-capture) Move to the MoveList using the given 'move' integer
static void addQuietMove(const Board& board, int move, MoveList *moveList) {
    ASSERT(squareOnBoard(GET_FROM(move)))
    ASSERT(squareOnBoard(GET_TO(move)))
    // MoveList's 'count' variable is essentially 'current index + 1'
    // So at the 'count' index in MoveList's Move array, set the Move's 'move' int to the given move
    moveList->moves[moveList->count].move = move;
    // Then set that Move's score. If move is in killer moves list, score accordingly, else score by searchHistory array!
    if (board.searchKillers[0][board.ply] == move) {
        moveList->moves[moveList->count].score = 900000;
    } else if (board.searchKillers[1][board.ply] == move) {
        moveList->moves[moveList->count].score = 800000;
    } else {
        moveList->moves[moveList->count].score = board.searchHistory[board.pieces[GET_FROM(move)]][GET_TO(move)];
    }
    moveList->count++; // Then update the counter
}

/// Adds a capturing Move to the MoveList using the given 'move' integer
static void addCaptureMove(const Board& board, int move, MoveList *moveList) {
    ASSERT(squareOnBoard(GET_FROM(move)))
    ASSERT(squareOnBoard(GET_TO(move)))
    ASSERT(pieceValid(GET_CAPTURED(move)))
    moveList->moves[moveList->count].move = move;
    moveList->moves[moveList->count].score = mVVLVAScores[GET_CAPTURED(move)][board.pieces[GET_FROM(move)]] + 1000000; // +1m makes sure these are searched first
    moveList->count++;
}

/// Adds an en passant Move to the MoveList using the given 'move' integer
static void addEnPassantMove(const Board& board, int move, MoveList *moveList) {
    ASSERT(squareOnBoard(GET_FROM(move)))
    ASSERT(squareOnBoard(GET_TO(move)))
    moveList->moves[moveList->count].move = move;
    moveList->moves[moveList->count].score = 105 + 1000000; // effectively pawn takes pawn. +1m makes sure these are searched first
    moveList->count++;
}

/// Add a capturing move by a white pawn to the Move list. Set 'captured' to EMPTY if it is not a capturing move
static void addWhitePawnCaptureMove(const Board& board, const int from, const int to, const int captured, MoveList *list) {
    ASSERT(pieceValidEmpty(captured))
    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))
    // if pawn starts on rank 7, then it will finish on rank 8, where it has to promote
    if (ranksBoard[from] == RANK_7) {
        addCaptureMove(board, MOVE(from, to, captured, wQ, 0), list);
        addCaptureMove(board, MOVE(from, to, captured, wR, 0), list);
        addCaptureMove(board, MOVE(from, to, captured, wB, 0), list);
        addCaptureMove(board, MOVE(from, to, captured, wN, 0), list);
    } else { // otherwise, add move with no promotion
        addCaptureMove(board, MOVE(from, to, captured, EMPTY, 0), list);
    }
}

/// Add a non-capturing move by a white pawn to the Move list
static void addWhitePawnMove(const Board& board, const int from, const int to, MoveList *list) {
    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))
    // if pawn starts on rank 7, then it will finish on rank 8, where it has to promote
    if (ranksBoard[from] == RANK_7) {
        addQuietMove(board, MOVE(from, to, EMPTY, wQ, 0), list);
        addQuietMove(board, MOVE(from, to, EMPTY, wR, 0), list);
        addQuietMove(board, MOVE(from, to, EMPTY, wB, 0), list);
        addQuietMove(board, MOVE(from, to, EMPTY, wN, 0), list);
    } else { // otherwise, add move with no promotion
        addQuietMove(board, MOVE(from, to, EMPTY, EMPTY, 0), list);
    }
}

/// Add a move by a black pawn to the Move list. Set 'captured' to EMPTY if it is not a capturing move
static void addBlackPawnCaptureMove(const Board& board, const int from, const int to, const int captured, MoveList *list) {
    ASSERT(pieceValidEmpty(captured))
    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))
    // if pawn starts on rank 2, then it will finish on rank 1, where it has to promote
    if (ranksBoard[from] == RANK_2) {
        addCaptureMove(board, MOVE(from, to, captured, bQ, 0), list);
        addCaptureMove(board, MOVE(from, to, captured, bR, 0), list);
        addCaptureMove(board, MOVE(from, to, captured, bB, 0), list);
        addCaptureMove(board, MOVE(from, to, captured, bN, 0), list);
    } else { // otherwise, add move with no promotion
        addCaptureMove(board, MOVE(from, to, captured, EMPTY, 0), list);
    }
}

/// Add a move by a black pawn to the Move list. Set 'captured' to EMPTY if it is not a capturing move
static void addBlackPawnMove(const Board& board, const int from, const int to, MoveList *list) {
    ASSERT(squareOnBoard(from))
    ASSERT(squareOnBoard(to))
    // if pawn starts on rank 2, then it will finish on rank 1, where it has to promote
    if (ranksBoard[from] == RANK_2) {
        addQuietMove(board, MOVE(from, to, EMPTY, bQ, 0), list);
        addQuietMove(board, MOVE(from, to, EMPTY, bR, 0), list);
        addQuietMove(board, MOVE(from, to, EMPTY, bB, 0), list);
        addQuietMove(board, MOVE(from, to, EMPTY, bN, 0), list);
    } else { // otherwise, add move with no promotion
        addQuietMove(board, MOVE(from, to, EMPTY, EMPTY, 0), list);
    }
}

void generateAllMoves(const Board& board, MoveList *moveList) {
    ASSERT(checkBoard(board))

    moveList->count = 0;
    int side = board.side;

    // PAWNS and CASTLING:
    // TODO: Consider collapsing this into a function call
    if (side == WHITE) {
        // Loop through each white pawn in the piece list, getting its square number...
        for (int pieceNum = 0; pieceNum < board.pieceCounts[wP]; ++pieceNum) {
            int square = board.pieceList[wP][pieceNum];
            ASSERT(squareOnBoard(square))
            // ...then generating NON-CAPTURE moves for it
            if (board.pieces[square+10] == EMPTY) { // +10 = move white forward 1 square
                addWhitePawnMove(board, square, square+10, moveList);
                if (ranksBoard[square] == RANK_2 && board.pieces[square+20] == EMPTY) { // +20 = move forward 2 squares = pawn start!
                    addQuietMove(board, MOVE(square, (square+20), EMPTY, EMPTY, MFLAG_PAWN_START), moveList);
                }
            }
            // ...then generating CAPTURE moves for it
            // (if square ahead and to the right/left is not offboard and has a black piece, add capture move):
            if (!SQUARE_OFFBOARD(square+9) && pieceColours[board.pieces[square + 9]] == BLACK) {
                addWhitePawnCaptureMove(board, square, square+9, board.pieces[square+9], moveList);
            }
            if (!SQUARE_OFFBOARD(square+11) && pieceColours[board.pieces[square + 11]] == BLACK) {
                addWhitePawnCaptureMove(board, square, square+11, board.pieces[square+11], moveList);
            }
            // ...then generating EN PASSANT moves for it
            if (board.enPasSq != NO_SQ) {
                if (square+9 == board.enPasSq) { // offboard check not needed, just check board's en passant square field!
                    addEnPassantMove(board, MOVE(square, square+9, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
                if (square+11 == board.enPasSq) {
                    addEnPassantMove(board, MOVE(square, square+11, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
            }
        }
        // WHITE CASTLING:
        // kingside - if bitwise ANDing the board's castling permission and the white kingside castling bit = true:
        if (board.castlingPerms & wK_CA) {
            // if squares between king and rook are empty: (do this first because less cpu intensive)
            if (board.pieces[F1] == EMPTY && board.pieces[G1] == EMPTY) {
                // if king and square next to king are not under attack (we check final position later):
                if ((!isSquareAttacked(E1, BLACK, board)) && (!isSquareAttacked(F1, BLACK, board))) {
                    // then add white kingside castling move
                    addQuietMove(board, MOVE(E1, G1, EMPTY, EMPTY, MFLAG_CASTLING), moveList);
                }
            }
        } // queenside (see above for comments):
        if (board.castlingPerms & wQ_CA) {
            if (board.pieces[D1] == EMPTY && board.pieces[C1] == EMPTY && board.pieces[B1] == EMPTY) {
                if ((!isSquareAttacked(E1, BLACK, board)) && (!isSquareAttacked(D1, BLACK, board))) {
                    addQuietMove(board, MOVE(E1, C1, EMPTY, EMPTY, MFLAG_CASTLING), moveList);
                }
            }
        }

    } else { // if side is BLACK (repeat of logic above)
        for (int pieceNum = 0; pieceNum < board.pieceCounts[bP]; ++pieceNum) { // loop through black pawns
            int square = board.pieceList[bP][pieceNum];
            ASSERT(squareOnBoard(square))
            // NON-CAPTURE moves:
            if (board.pieces[square-10] == EMPTY) { // -10 = move black forward 1 square
                addBlackPawnMove(board, square, square-10, moveList);
                if (ranksBoard[square] == RANK_7 && board.pieces[square-20] == EMPTY) { // -20 = move forward 2 squares = pawn start!
                    addQuietMove(board, MOVE(square, (square-20), EMPTY, EMPTY, MFLAG_PAWN_START), moveList);
                }
            }
            // CAPTURE moves:
            if (!SQUARE_OFFBOARD(square-9) && pieceColours[board.pieces[square - 9]] == WHITE) {
                addBlackPawnCaptureMove(board, square, square-9, board.pieces[square-9], moveList);
            }
            if (!SQUARE_OFFBOARD(square-11) && pieceColours[board.pieces[square - 11]] == WHITE) {
                addBlackPawnCaptureMove(board, square, square-11, board.pieces[square-11], moveList);
            }
            if (board.enPasSq != NO_SQ) {
                // EN PASSANT moves:
                if (square - 9 == board.enPasSq) { // offboard check not needed, just check board's en passant square field!
                    addEnPassantMove(board, MOVE(square, square - 9, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
                if (square - 11 == board.enPasSq) {
                    addEnPassantMove(board, MOVE(square, square - 11, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
            }
        }
        // BLACK CASTLING:
        // kingside(see white castling for comments)
        if (board.castlingPerms & bK_CA) {
            if (board.pieces[F8] == EMPTY && board.pieces[G8] == EMPTY) {
                if ((!isSquareAttacked(E8, WHITE, board)) && (!isSquareAttacked(F8, WHITE, board))) {
                    addQuietMove(board, MOVE(E8, G8, EMPTY, EMPTY, MFLAG_CASTLING), moveList);
                }
            }
        } // queenside (see white castling for comments):
        if (board.castlingPerms & bQ_CA) {
            if (board.pieces[D8] == EMPTY && board.pieces[C8] == EMPTY && board.pieces[B8] == EMPTY) {
                if ((!isSquareAttacked(E8, WHITE, board)) && (!isSquareAttacked(D8, WHITE, board))) {
                    addQuietMove(board, MOVE(E8, C8, EMPTY, EMPTY, MFLAG_CASTLING), moveList);
                }
            }
        }
    }

    // SLIDING moves:
    int pieceIndex = slideLoopStartIndex[side]; // todo: is doing this with an array really better than '(side==WHITE) ? 0 : 4'?
    int piece = slidePiecesArray[pieceIndex++];
    while (piece != 0) { // for each type of sliding piece of the side's colour todo: loop optimisation here?
        ASSERT(pieceValid(piece))

        for (int pieceNum = 0; pieceNum < board.pieceCounts[piece]; ++pieceNum) { // for each instance of this piece on the board
            int square = board.pieceList[piece][pieceNum]; // get the piece's 120-based square index
            ASSERT(squareOnBoard(square))

            for (int i = 0; i < pieceDirectionNumbers[piece]; ++i) { // for each direction this SLIDING piece can move in
                int direction = pieceDirection[piece][i];
                int targetSquare = square + direction; // calculate the 'target' square

                while (!SQUARE_OFFBOARD(targetSquare)) { // while the square is not offboard
                    // Determine if the target square contains a piece of the opposite colour:
                    if (board.pieces[targetSquare] != EMPTY) {
                        if (pieceColours[board.pieces[targetSquare]] == (side ^ 1)) { // exclusive ORing the side int with 1 gives the opposite side!
                            addCaptureMove(board, MOVE(square, targetSquare, board.pieces[targetSquare], EMPTY, 0), moveList);
                        }
                        break; // else, break out of moving in this direction because the square wasn't empty or capturable
                    } // but can move if square was empty:
                    addQuietMove(board, MOVE(square, targetSquare, EMPTY, EMPTY, 0), moveList);
                    targetSquare += direction; // keep moving in this direction
                }
            }
        }
        piece = slidePiecesArray[pieceIndex++];
    }

    // NON-SLIDING moves:
    pieceIndex = nonSlideLoopStartIndex[side];
    piece = nonSlidePiecesArray[pieceIndex++];
    while (piece != 0) { // for each type of non-sliding piece of the side's colour
        ASSERT(pieceValid(piece))

        for (int pieceNum = 0; pieceNum < board.pieceCounts[piece]; ++pieceNum) { // for each instance of this piece on the board
            int square = board.pieceList[piece][pieceNum]; // get the piece's 120-based square index
            ASSERT(squareOnBoard(square))

            for (int i = 0; i < pieceDirectionNumbers[piece]; ++i) { // for each direction this NON-SLIDING piece can move in
                int direction = pieceDirection[piece][i];
                int targetSquare = square + direction; // calculate the 'target' square

                if (SQUARE_OFFBOARD(targetSquare)) {
                    continue; // check if the target is off the board: move on to the next direction if so
                }
                // Determine if the target square contains a piece of the opposite colour:
                if (board.pieces[targetSquare] != EMPTY) {
                    if (pieceColours[board.pieces[targetSquare]] == (side ^ 1)) { // exclusive ORing the side with 1 gives the opposite side!
                        addCaptureMove(board, MOVE(square, targetSquare, board.pieces[targetSquare], EMPTY, 0), moveList);
                    }
                    continue; // else, move to next piece because the square wasn't empty or capturable
                } // but can move if square was empty:
                addQuietMove(board, MOVE(square, targetSquare, EMPTY, EMPTY, 0), moveList);
            }
        }
        piece = nonSlidePiecesArray[pieceIndex++];
    }
}

void generateAllCaptureMoves(const Board& board, MoveList *moveList) {
    ASSERT(checkBoard(board))

    moveList->count = 0;
    int side = board.side;

    // PAWNS and CASTLING:
    // TODO: Consider collapsing this into a function call
    if (side == WHITE) {
        // Loop through each white pawn in the piece list, getting its square number...
        for (int pieceNum = 0; pieceNum < board.pieceCounts[wP]; ++pieceNum) {
            int square = board.pieceList[wP][pieceNum];
            ASSERT(squareOnBoard(square))
            // generate CAPTURE moves for it
            // (if square ahead and to the right/left is not offboard and has a black piece, add capture move):
            if (!SQUARE_OFFBOARD(square+9) && pieceColours[board.pieces[square + 9]] == BLACK) {
                addWhitePawnCaptureMove(board, square, square+9, board.pieces[square+9], moveList);
            }
            if (!SQUARE_OFFBOARD(square+11) && pieceColours[board.pieces[square + 11]] == BLACK) {
                addWhitePawnCaptureMove(board, square, square+11, board.pieces[square+11], moveList);
            }
            // ...then generating EN PASSANT moves for it
            if (board.enPasSq != NO_SQ) {
                if (square+9 == board.enPasSq) { // offboard check not needed, just check board's en passant square field!
                    addEnPassantMove(board, MOVE(square, square+9, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
                if (square+11 == board.enPasSq) {
                    addEnPassantMove(board, MOVE(square, square+11, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
            }
        }
    } else { // if side is BLACK (repeat of logic above)
        for (int pieceNum = 0; pieceNum < board.pieceCounts[bP]; ++pieceNum) { // loop through black pawns
            int square = board.pieceList[bP][pieceNum];
            ASSERT(squareOnBoard(square))
            // CAPTURE moves:
            if (!SQUARE_OFFBOARD(square-9) && pieceColours[board.pieces[square - 9]] == WHITE) {
                addBlackPawnCaptureMove(board, square, square-9, board.pieces[square-9], moveList);
            }
            if (!SQUARE_OFFBOARD(square-11) && pieceColours[board.pieces[square - 11]] == WHITE) {
                addBlackPawnCaptureMove(board, square, square-11, board.pieces[square-11], moveList);
            }
            if (board.enPasSq != NO_SQ) {
                // EN PASSANT moves:
                if (square - 9 == board.enPasSq) { // offboard check not needed, just check board's en passant square field!
                    addEnPassantMove(board, MOVE(square, square - 9, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
                if (square - 11 == board.enPasSq) {
                    addEnPassantMove(board, MOVE(square, square - 11, EMPTY, EMPTY, MFLAG_EN_PASSANT), moveList);
                }
            }
        }
    }

    // SLIDING moves:
    int pieceIndex = slideLoopStartIndex[side]; // todo: is doing this with an array really better than '(side==WHITE) ? 0 : 4'?
    int piece = slidePiecesArray[pieceIndex++];
    while (piece != 0) { // for each type of sliding piece of the side's colour todo: loop optimisation here?
        ASSERT(pieceValid(piece))

        for (int pieceNum = 0; pieceNum < board.pieceCounts[piece]; ++pieceNum) { // for each instance of this piece on the board
            int square = board.pieceList[piece][pieceNum]; // get the piece's 120-based square index
            ASSERT(squareOnBoard(square))

            for (int i = 0; i < pieceDirectionNumbers[piece]; ++i) { // for each direction this SLIDING piece can move in
                int direction = pieceDirection[piece][i];
                int targetSquare = square + direction; // calculate the 'target' square

                while (!SQUARE_OFFBOARD(targetSquare)) { // while the square is not offboard
                    // Determine if the target square contains a piece of the opposite colour:
                    if (board.pieces[targetSquare] != EMPTY) {
                        if (pieceColours[board.pieces[targetSquare]] == (side ^ 1)) { // exclusive ORing the side int with 1 gives the opposite side!
                            addCaptureMove(board, MOVE(square, targetSquare, board.pieces[targetSquare], EMPTY, 0), moveList);
                        }
                        break; // else, break out of moving in this direction because the square wasn't empty or capturable
                    } // but can move if square was empty:
                    targetSquare += direction; // keep moving in this direction
                }
            }
        }
        piece = slidePiecesArray[pieceIndex++];
    }

    // NON-SLIDING moves:
    pieceIndex = nonSlideLoopStartIndex[side];
    piece = nonSlidePiecesArray[pieceIndex++];
    while (piece != 0) { // for each type of non-sliding piece of the side's colour
        ASSERT(pieceValid(piece))

        for (int pieceNum = 0; pieceNum < board.pieceCounts[piece]; ++pieceNum) { // for each instance of this piece on the board
            int square = board.pieceList[piece][pieceNum]; // get the piece's 120-based square index
            ASSERT(squareOnBoard(square))

            for (int i = 0; i < pieceDirectionNumbers[piece]; ++i) { // for each direction this NON-SLIDING piece can move in
                int direction = pieceDirection[piece][i];
                int targetSquare = square + direction; // calculate the 'target' square

                if (SQUARE_OFFBOARD(targetSquare)) {
                    continue; // check if the target is off the board: move on to the next direction if so
                }
                // Determine if the target square contains a piece of the opposite colour:
                if (board.pieces[targetSquare] != EMPTY) {
                    if (pieceColours[board.pieces[targetSquare]] == (side ^ 1)) { // exclusive ORing the side with 1 gives the opposite side!
                        addCaptureMove(board, MOVE(square, targetSquare, board.pieces[targetSquare], EMPTY, 0), moveList);
                    }
                    continue; // else, move to next piece because the square wasn't empty or capturable
                } // but can move if square was empty:
            }
        }
        piece = nonSlidePiecesArray[pieceIndex++];
    }
}

void initMVVLVA() {
    for (int attacker = wP; attacker <= bK; ++attacker) {
        for (int victim = wP; victim <= bK; ++victim) {
            mVVLVAScores[victim][attacker] = (victimScore[victim] + 6) - (victimScore[attacker] / 100);
            // e.g. (queen + 6 - pawn)  ==  (500 + 6 - (100/100))  ==  (506 - 1)  ==  (505)
        }
    }
}

/// Check that a move is legal given the board state
int moveExists(Board& board, const int move) {
    MoveList moveList[1];
    generateAllMoves(board, moveList);
    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        if (!makeMove(board, moveList->moves[moveNum].move)) {
            continue;
        }
        undoMove(board);
        if (moveList->moves[moveNum].move == move) {
            return true;
        }
    }
    return false;
}
