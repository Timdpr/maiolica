#include "defs.h"
#include <cstdio>

/// Lots of cross-checking of most elements of the board! (only in debug builds, where ASSERT does something)
int checkBoard(const Board& board) {
#ifdef DEBUG
    // Check piece lists match
    for (int piece = wP; piece <= bK; ++piece) {
        for (int pieceNum = 0; pieceNum < board.pieceCounts[piece]; ++pieceNum) {
            int square120 = board.pieceList[piece][pieceNum];
            ASSERT(board.pieces[square120] == piece)
            ASSERT(board.pieceIndex[square120] == pieceNum)
        }
    }

    // Set up various counters
    int expectedPieceNums[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    // sized 3 since empty squares have colour BOTH
    int expectedBigPieces[3] = { 0, 0, 0 };
    int expectedMajorPieces[3] = { 0, 0, 0 };
    int expectedMinorPieces[3] = { 0, 0, 0 };
    int expectedMaterial[3] = { 0, 0, 0 };

    for (int square64 = 0; square64 < 64; ++square64) {
        int square120 = INDEX_64_TO_120(square64);
        int piece = board.pieces[square120]; // go through board pieces array
        expectedPieceNums[piece]++; // increment number of this piece
        int colour = pieceColours[piece];
        // add to piece type counts
        if (bigPieces[piece] == true) expectedBigPieces[colour]++;
        if (minorPieces[piece] == true) expectedMinorPieces[colour]++;
        if (majorPieces[piece] == true) expectedMajorPieces[colour]++;

        expectedMaterial[colour] += pieceValues[piece]; // add to side's material score
    }
    // Check piece number array
    for (int piece = wP; piece <= bK; ++piece) {
        ASSERT(expectedPieceNums[piece] == board.pieceCounts[piece])
    }

    // Check pawn bitboard
    U64 expectedPawns[3] = {U64(0), U64(0), U64(0)};
    expectedPawns[WHITE] = board.pawns[WHITE];
    expectedPawns[BLACK] = board.pawns[BLACK];
    expectedPawns[BOTH] = board.pawns[BOTH];

    int pawnCount = countBits(expectedPawns[WHITE]);
    ASSERT(pawnCount == board.pieceCounts[wP])
    pawnCount = countBits(expectedPawns[BLACK]);
    ASSERT(pawnCount == board.pieceCounts[bP])
    pawnCount = countBits(expectedPawns[BOTH]);
    ASSERT(pawnCount == board.pieceCounts[wP] + board.pieceCounts[bP])

    // Check bitboard squares
    while (expectedPawns[WHITE]) {
        int square64 = popBit(&expectedPawns[WHITE]);
        ASSERT(board.pieces[INDEX_64_TO_120(square64)] == wP)
    }
    while (expectedPawns[BLACK]) {
        int square64 = popBit(&expectedPawns[BLACK]);
        ASSERT(board.pieces[INDEX_64_TO_120(square64)] == bP)
    }
    while (expectedPawns[BOTH]) {
        int square64 = popBit(&expectedPawns[BOTH]);
        ASSERT( (board.pieces[INDEX_64_TO_120(square64)] == bP) || (board.pieces[INDEX_64_TO_120(square64)] == wP) )
    }

    // Check various counts, etc.
    ASSERT(expectedMaterial[WHITE] == board.material[WHITE] && expectedMaterial[BLACK] == board.material[BLACK])
    ASSERT(expectedMinorPieces[WHITE] == board.minorPieces[WHITE] && expectedMinorPieces[BLACK] == board.minorPieces[BLACK])
    ASSERT(expectedMajorPieces[WHITE] == board.majorPieces[WHITE] && expectedMajorPieces[BLACK] == board.majorPieces[BLACK])
    ASSERT(expectedBigPieces[WHITE] == board.bigPieces[WHITE] && expectedBigPieces[BLACK] == board.bigPieces[BLACK])

    ASSERT(board.side == WHITE || board.side == BLACK)
    ASSERT(generatePositionKey(board) == board.positionKey)

    ASSERT(board.enPasSq==NO_SQ || ( ranksBoard[board.enPasSq] == RANK_6 && board.side == WHITE)
           || ( ranksBoard[board.enPasSq] == RANK_3 && board.side == BLACK)) // en passant specifics!

    ASSERT(board.pieces[board.kingSq[WHITE]] == wK)
    ASSERT(board.pieces[board.kingSq[BLACK]] == bK)
#endif
    return true;
}

/// Loops through all pieces and populates various lists of 'material'
void updateMaterialLists(Board& board) {
    for (int square120 = 0; square120 < BRD_SQ_NUM; ++square120) { // Loop through pieces
        int piece = board.pieces[square120];
        if (piece != OFFBOARD && piece != EMPTY) {
            int colour = pieceColours[piece];
            // Piece type counts
            if (bigPieces[piece] == true) board.bigPieces[colour]++;
            if (minorPieces[piece] == true) board.minorPieces[colour]++;
            if (majorPieces[piece] == true) board.majorPieces[colour]++;
            // Overall material score
            board.material[colour] += pieceValues[piece];
            // Square index of each piece, & where to find it in the list
            board.pieceList[piece][board.pieceCounts[piece]] = square120;
            board.pieceIndex[square120] = board.pieceCounts[piece];
            // How many of each type of piece
            board.pieceCounts[piece]++;
            // King locations
            if (piece == wK) board.kingSq[WHITE] = square120;
            if (piece == bK) board.kingSq[BLACK] = square120;
            // Set bitboards for pawns
            if (piece == wP) {
                SET_BIT(board.pawns[WHITE], INDEX_120_TO_64(square120));
                SET_BIT(board.pawns[BOTH], INDEX_120_TO_64(square120));
            } else if (piece == bP) {
                SET_BIT(board.pawns[BLACK], INDEX_120_TO_64(square120));
                SET_BIT(board.pawns[BOTH], INDEX_120_TO_64(square120));
            }
        }
    }
}

/** Parses a Forsyth–Edwards Notation string and sets the given board with it.
 *  Returns -1 (leaving a half-built board) if the FEN is malformed
 */
int parseFen(const char *fen, Board& board) {
    ASSERT(fen != nullptr)

    int rank = RANK_8;
    int file = FILE_A;
    int piece = 0;
    int pieceNums[13] = {}; // so we can refuse more than 10 of a piece (pieceList holds 10) or 1 king

    resetBoard(board);

    while ((rank >= RANK_1) && *fen) {
        int count = 1;
        switch (*fen) {
            // is it a piece? Set piece var to enum value.
            case 'p': piece = bP; break;
            case 'r': piece = bR; break;
            case 'n': piece = bN; break;
            case 'b': piece = bB; break;
            case 'k': piece = bK; break;
            case 'q': piece = bQ; break;
            case 'P': piece = wP; break;
            case 'R': piece = wR; break;
            case 'N': piece = wN; break;
            case 'B': piece = wB; break;
            case 'K': piece = wK; break;
            case 'Q': piece = wQ; break;
            // is it a count of empty squares? Set piece var to EMPTY and set count var to int value of char
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
                piece = EMPTY;
                count = *fen - '0'; // subtracting ASCII values of characters
                break;
            // are we at the end of a rank? Decrement rank counter, reset file, increment fen pointer and try again
            case '/':
            case ' ':
                // every rank needs exactly 8 squares, and the space has to come right after rank 1
                if (file != 8 || (*fen == ' ') != (rank == RANK_1)) return -1;
                rank--;
                file = FILE_A;
                fen++;
                continue;
            // if everything above is missed, then we have an error!
            default:
                return -1;
        }

        if (file + count > 8) return -1; // too many squares in this rank
        if ((piece == wP || piece == bP) && (rank == RANK_1 || rank == RANK_8)) return -1;
        if (piece != EMPTY && ++pieceNums[piece] > ((piece == wK || piece == bK) ? 1 : 10)) return -1;

        // empty piece 'count' is set to 1 OR the number of squares to skip,
        // so this will set piece OR skip the required amount of times!
        for (int i = 0; i < count; i++) {
            if (piece != EMPTY) {
                board.pieces[INDEX_64_TO_120(rank * 8 + file)] = piece;
            }
            file++;
        }
        fen++;
    }
    // we should have had all 8 ranks, and one king each
    if (rank >= RANK_1 || pieceNums[wK] != 1 || pieceNums[bK] != 1) return -1;
    // and room in pieceList for every pawn to promote to the same piece (true of any legal position)
    for (int white = wN, black = bN; white <= wQ; white++, black++) {
        if (pieceNums[white] + pieceNums[wP] > 10 || pieceNums[black] + pieceNums[bP] > 10) return -1;
    }

    // At this point, our fen pointer should be at the 'side to move' position
    if ((*fen != 'w' && *fen != 'b') || (fen[1] != ' ' && fen[1] != '\0')) return -1;
    board.side = (*fen == 'w') ? WHITE : BLACK;
    fen++;
    if (*fen) fen++; // everything after the side is optional, so never step past the end

    // Now castling permissions
    for (int i = 0; i < 4; i++) { // could be up to 4 chars here
        if (*fen == ' ' || *fen == '\0') {
            break;
        }
        switch(*fen) {
            case 'K': board.castlingPerms |= wK_CA; break;
            case 'Q': board.castlingPerms |= wQ_CA; break;
            case 'k': board.castlingPerms |= bK_CA; break;
            case 'q': board.castlingPerms |= bQ_CA; break;
            default: break; // if we get a dash, move on
        }
        fen++;
    }
    if (*fen) fen++;
    // drop any right whose king or rook isn't on its home square, or movegen would castle phantom pieces
    if (board.pieces[E1] != wK) board.castlingPerms &= ~(wK_CA | wQ_CA);
    if (board.pieces[H1] != wR) board.castlingPerms &= ~wK_CA;
    if (board.pieces[A1] != wR) board.castlingPerms &= ~wQ_CA;
    if (board.pieces[E8] != bK) board.castlingPerms &= ~(bK_CA | bQ_CA);
    if (board.pieces[H8] != bR) board.castlingPerms &= ~bK_CA;
    if (board.pieces[A8] != bR) board.castlingPerms &= ~bQ_CA;
    ASSERT(board.castlingPerms >= 0 && board.castlingPerms <= 15)

    // En passant square:
    if (*fen && *fen != '-') { // if we have an en passant square...
        file = fen[0] - 'a'; // set file and rank ints with ASCII trick
        rank = fen[1] - '1';

        // only keep it if it's empty and just behind a pawn that double pushed: rank 6 with white to move, 3 with black
        if (file >= FILE_A && file <= FILE_H && rank == ((board.side == WHITE) ? RANK_6 : RANK_3)) {
            int sq = FILE_RANK_TO_SQUARE_INDEX(file, rank);
            int pushedSq = (board.side == WHITE) ? sq - 10 : sq + 10;
            if (board.pieces[sq] == EMPTY && board.pieces[pushedSq] == ((board.side == WHITE) ? bP : wP)) {
                board.enPasSq = sq; // set square
            }
        }
    }

    // Halfmove clock, if it's there (the fullmove number isn't needed): skip the ep field and spaces first
    while (*fen && *fen != ' ') fen++;
    while (*fen == ' ') fen++;
    int halfMoves = 0;
    for (; *fen >= '0' && *fen <= '9' && halfMoves <= 100; fen++) {
        halfMoves = halfMoves * 10 + (*fen - '0');
    }
    board.fiftyMove = (halfMoves > 100) ? 100 : halfMoves;

    // Generate and set position (hash) key
    board.positionKey = generatePositionKey(board);

    updateMaterialLists(board);

    return 0;
}

/// Resets all attributes of the Board!
void resetBoard(Board& board) {
    for (int & piece : board.pieces) {
        piece = OFFBOARD;
    }

    for (int i = 0; i < 64; ++i) {
        board.pieces[INDEX_64_TO_120(i)] = EMPTY;
    }

    for (int i = 0; i < 2; ++i) {
        board.bigPieces[i] = 0;
        board.majorPieces[i] = 0;
        board.minorPieces[i] = 0;
        board.material[i] = 0;
    }

    for (U64& pawn : board.pawns) {
        pawn = U64(0);
    }

    for (int & pieceCount : board.pieceCounts) {
        pieceCount = 0;
    }

    board.kingSq[WHITE] = board.kingSq[BLACK] = NO_SQ;

    board.side = BOTH;
    board.enPasSq = NO_SQ;
    board.fiftyMove = 0;

    board.ply = 0;
    board.historyPly = 0;

    board.castlingPerms = 0;

    board.positionKey = U64(0);
}

void printBoard(const Board& board) {
    std::printf("\n Board:\n");

    for(int rank = RANK_8; rank >= RANK_1; rank--) {
        for(int file = FILE_A; file <= FILE_H; file++) {
            int sq = FILE_RANK_TO_SQUARE_INDEX(file, rank);
            int piece = board.pieces[sq];
            std::printf("%3c", pieceChars[piece]);
        }
        std::printf("   %d\n", rank+1);
    }

    std::printf("\n");
    for(int file = FILE_A; file <= FILE_H; file++) {
        std::printf("%3c", 'A'+file);
    }
    std::printf("\n\n");
    std::printf("side character: %c\n", sideChars[board.side]);
    std::printf("en passant square: %d\n", board.enPasSq);
    std::printf("castling permissions: %c%c%c%c\n",
            (board.castlingPerms & wK_CA) ? 'K' : '-',
            (board.castlingPerms & wQ_CA) ? 'Q' : '-',
            (board.castlingPerms & bK_CA) ? 'k' : '-',
            (board.castlingPerms & bQ_CA) ? 'q' : '-'
    );
    std::printf("position key: %llX\n", (unsigned long long)board.positionKey);
}

void mirrorBoard(Board& board) {
    int tempPiecesArray[64];
    int tempSide = board.side^1;
    int swapPiece[13] = { EMPTY, bP, bN, bB, bR, bQ, bK, wP, wN, wB, wR, wQ, wK }; // swapped pieces enum...
    int tempCastlingPerms = 0;
    int tempEnPasSq = NO_SQ;

    // Swap castling permissions
    if (board.castlingPerms & wK_CA) tempCastlingPerms |= bK_CA;
    if (board.castlingPerms & wQ_CA) tempCastlingPerms |= bQ_CA;
    if (board.castlingPerms & bK_CA) tempCastlingPerms |= wK_CA;
    if (board.castlingPerms & bQ_CA) tempCastlingPerms |= wQ_CA;

    // Swap en passant squares (tricky with indexing...)
    if (board.enPasSq != NO_SQ) {
        tempEnPasSq = INDEX_64_TO_120(mirror64[INDEX_120_TO_64(board.enPasSq)]);
    }
    // Make temp pieces array for future swapping (tricky with indexing...)
    for (int square = 0; square < 64; square++) {
        tempPiecesArray[square] = board.pieces[INDEX_64_TO_120(mirror64[square])];
    }

    resetBoard(board);

    // Now swap pieces using temp array and swapped pieces array/enum
    for (int square = 0; square < 64; square++) {
        board.pieces[INDEX_64_TO_120(square)] = swapPiece[tempPiecesArray[square]];
    }

    //
    board.side = tempSide;
    board.castlingPerms = tempCastlingPerms;
    board.enPasSq = tempEnPasSq;
    board.positionKey = generatePositionKey(board);

    updateMaterialLists(board);

    ASSERT(checkBoard(board))
}
