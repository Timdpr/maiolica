#include "defs.h"
#include <cstdio>

/// Lots of cross-checking of most elements of the board!
int checkBoard(const Board& board) {
    // Check piece lists match
    for (int piece = wP; piece <= bK; ++piece) {
        for (int pieceNum = 0; pieceNum < board.pieceCounts[piece]; ++pieceNum) {
            int square120 = board.pieceList[piece][pieceNum];
            ASSERT(board.pieces[square120] == piece)
        }
    }

    // Set up various counters
    int expectedPieceNums[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int expectedBigPieces[2] = { 0, 0 };
    int expectedMajorPieces[2] = { 0, 0 };
    int expectedMinorPieces[2] = { 0, 0 };
    int expectedMaterial[2] = { 0, 0 };

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

    int pawnCount = COUNT(expectedPawns[WHITE]);
    ASSERT(pawnCount == board.pieceCounts[wP])
    pawnCount = COUNT(expectedPawns[BLACK]);
    ASSERT(pawnCount == board.pieceCounts[bP])
    pawnCount = COUNT(expectedPawns[BOTH]);
    ASSERT(pawnCount == board.pieceCounts[wP] + board.pieceCounts[bP])

    // Check bitboard squares
    while (expectedPawns[WHITE]) {
        int square64 = POP(&expectedPawns[WHITE]);
        ASSERT(board.pieces[INDEX_64_TO_120(square64)] == wP)
    }
    while (expectedPawns[BLACK]) {
        int square64 = POP(&expectedPawns[BLACK]);
        ASSERT(board.pieces[INDEX_64_TO_120(square64)] == bP)
    }
    while (expectedPawns[BOTH]) {
        int square64 = POP(&expectedPawns[BOTH]);
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
            // Square index of each piece
            board.pieceList[piece][board.pieceCounts[piece]] = square120;
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

/** Parses a Forsyth–Edwards Notation string and sets the given board with it
 *  TODO: Missing half move clock!!!
 */
int parseFen(const char *fen, Board& board) {
    ASSERT(fen != nullptr)

    int rank = RANK_8;
    int file = FILE_A;
    int piece = 0;

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
                rank--;
                file = FILE_A;
                fen++;
                continue;
            // if everything above is missed, then we have an error!
            default:
                std::printf("FEN error! \n");
                return -1;
        }

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
    // At this point, our fen pointer should be at the 'side to move' position
    ASSERT(*fen == 'w' || *fen == 'b')
    board.side = (*fen == 'w') ? WHITE : BLACK;
    fen += 2;

    // Now castling permissions
    for (int i = 0; i < 4; i++) { // could be up to 4 chars here
        if (*fen == ' ') {
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
    fen++;
    ASSERT(board.castlingPerms >= 0 && board.castlingPerms <= 15)

    // En passant square:
    if (*fen != '-') { // if we have an en passant square...
        file = fen[0] - 'a'; // set file and rank ints with ASCII trick
        rank = fen[1] - '1';

        ASSERT(file >= FILE_A && file <= FILE_H)
        ASSERT(rank >= RANK_1 && rank <= RANK_8)

        board.enPasSq = FILE_RANK_TO_SQUARE_INDEX(file, rank); // set square
    }

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

    for (unsigned long long & pawn : board.pawns) {
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

    // TODO: Missing resetting material?
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
    std::printf("position key: %llX\n", board.positionKey);
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

Board& genBoard() {
    static Board board;
    board.hashTable->hTable = nullptr;
    return board;
}

//Board *genBoard() {
//    Board *board = (Board*)malloc(sizeof(Board));
//    board->hashTable->hTable = nullptr;
//    return board;
//}
