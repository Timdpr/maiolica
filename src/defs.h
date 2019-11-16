#ifndef MAIOLICA_DEFS_H
#define MAIOLICA_DEFS_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

//#define DEBUG

#ifndef DEBUG
#define ASSERT(n)
#else
#define ASSERT(n) \
if(!(n)) { \
std::printf("\n*** ASSERTION ERROR ***\n%s : FAILED ",#n); \
std::printf("on %s ",__DATE__); \
std::printf("at %s \n",__TIME__); \
std::printf("At line %d ",__LINE__); \
std::printf("in file %s \n",__FILE__); \
std::exit(0);}
#endif //DEBUG

#define NAME "Maiolica 1.0"
#define BRD_SQ_NUM 120 // number of squares on the main, 'mailbox representation' board
#define MAX_GAME_MOVES 2048
#define MAX_POSITION_MOVES 256
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

typedef std::uint64_t U64;


/* -- ENUMS -- */
enum { EMPTY, wP, wN, wB, wR, wQ, wK, bP, bN, bB, bR, bQ, bK };
enum { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NONE };
enum { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NONE };

enum { WHITE, BLACK, BOTH };

enum {
    A1 = 21, B1, C1, D1, E1, F1, G1, H1,
    A2 = 31, B2, C2, D2, E2, F2, G2, H2,
    A3 = 41, B3, C3, D3, E3, F3, G3, H3,
    A4 = 51, B4, C4, D4, E4, F4, G4, H4,
    A5 = 61, B5, C5, D5, E5, F5, G5, H5,
    A6 = 71, B6, C6, D6, E6, F6, G6, H6,
    A7 = 81, B7, C7, D7, E7, F7, G7, H7,
    A8 = 91, B8, C8, D8, E8, F8, G8, H8, NO_SQ, OFFBOARD
};

enum { FALSE, TRUE };

enum { wK_CA = 1, wQ_CA = 2, bK_CA = 4, bQ_CA = 8 }; // Castling permissions


/* -- STRUCTS -- */
/** Uses an integer to store all necessary information, and another int for the score

0000 0000 0000 0000 0000 0111 1111 -> 'From' square: 0x7F                         <br>
0000 0000 0000 0011 1111 1000 0000 -> 'To' square: >>7, 0x7F                      <br>
0000 0000 0011 1100 0000 0000 0000 -> Piece captured, if any: >>14, 0xF           <br>
0000 0000 0100 0000 0000 0000 0000 -> En passant?: bit and, 0x40000               <br>
0000 0000 1000 1100 0000 0000 0000 -> Pawn start move?: bit and, 0x80000          <br>
0000 1111 0000 0000 0000 0000 0000 -> What piece was promoted, if any: >>20, 0xF  <br>
0001 0000 0000 0000 0000 0000 0000 -> Was it a castling move?: bit and, 0x1000000

We can also check '0x7C000' to see if a move was a capturing move, and 0xF00000 to see if it contains a promotion */
struct Move {
    int move; // stores lots of information
    int score; // used for move ordering
};

struct MoveList {
    Move moves[MAX_POSITION_MOVES];
    int count;
};

/// Holds game state at a particular ply
struct Undo {
    int move;
    int castlingPerms;
    int enPasSq;
    int fiftyMove;
    U64 positionKey;
};

/// Representation of a board position
struct Board {
    int pieces[BRD_SQ_NUM]; // array of pieces indexed by 120-based square index
    U64 pawns[3]; // bitboard
    int kingSq[2]; // square that the king is on, indexed by side

    int side; // the side to play, WHITE or BLACK
    int enPasSq; // en passant square, if set
    int fiftyMove; // 'fifty move rule' counter

    int ply;
    int historyPly;

    int castlingPerms; // castling permissions, from white kingside to black queenside, held in 4 bits (e.g. 15 == 1111 == all true)

    U64 positionKey; // hashkey

    int pieceCounts[13]; // holds numbers of types pieces currently on the board (index corresponds to piece type enum)
    int bigPieces[2]; // # of non-pawn pieces by colour
    int majorPieces[2]; // # of rooks & queens by colour
    int minorPieces[2]; // # of bishops & knights by colour
    int material[2]; // total piece score for a side

    Undo history[MAX_GAME_MOVES]; // holds game state (via 'Undo' struct) at each ply

    int pieceList[13][10]; // 13 types of pieces, and at most 10 of each // TODO: May need to convert this to 1D for speed
};


/* -- MACROS -- */
// - Square index macros:
/// Takes 64-based file and rank index and returns 120-based square index
#define FILE_RANK_TO_SQUARE_INDEX(file, rank) ( (21 + (file)) + ((rank) * 10) )
/// Takes 120-based square index and returns equivalent 64-based square index
#define INDEX_120_TO_64(sq120) (sq120ToSq64[(sq120)])
/// Takes 64-based square index and returns equivalent 120-based square index
#define INDEX_64_TO_120(sq64) (sq64ToSq120[(sq64)])
/// Finds first (least significant) bit in given bitboard, and returns index

// - Bitboard macros:
///
#define POP(bitboard) popBit(bitboard)
/// Returns number of bits in bitboard
#define COUNT(bitboard) countBits(bitboard)
///
#define CLEAR_BIT(bitboard, sq) ((bitboard) &= clearMask[(sq)])
///
#define SET_BIT(bitboard, sq) ((bitboard) |= setMask[(sq)])

// - Piece checking macros:
#define IS_KNIGHT(piece) (pieceKnight[(piece)])
#define IS_BISHOP_OR_QUEEN(piece) (pieceBishopQueen[(piece)])
#define IS_ROOK_OR_QUEEN(piece) (pieceRookQueen[(piece)])
#define IS_KING(piece) (pieceKing[(piece)])

// - Operations on Move int macros:
// For getting info out of Move integer:
/// Get 'from' square from move int
#define GET_FROM(move) ((move) & 0x7F)
/// Get 'to' square from move int
#define GET_TO(move) (((move)>>7) & 0x7F)
/// Get captured piece from move int
#define GET_CAPTURED(move) (((move)>>14) & 0xF)
/// Get promoted piece from move int
#define GET_PROMOTED(move) (((move)>>20) & 0xF)
/// Set en passant flag in move by ORing in this flag
#define MFLAG_EN_PASSANT 0x40000
/// Set pawn start flag in move by ORing in this flag
#define MFLAG_PAWN_START 0x80000
/// Set castling flag in move by ORing in this flag
#define MFLAG_CASTLING 0x1000000
/// Set capturing flag in move by ORing in this flag
#define MFLAG_CAPTURING(move) 0x7C000
/// Set promotion flag in move by ORing in this flag
#define MFLAG_PROMOTION(move) 0xF00000

/* -- GLOBAL -- */  // TODO: Consider making these non-global...
extern U64 RAND_64;

extern int sq120ToSq64[BRD_SQ_NUM];
extern int sq64ToSq120[64];

extern U64 setMask[64];
extern U64 clearMask[64];
extern U64 pieceKeys[13][120];
extern U64 sideKey;
extern U64 castleKeys[16];

extern const char pieceChars[];
extern const char sideChars[];
extern const char rankChars[];
extern const char fileChars[];

extern const int bigPieces[13]; // stores whether piece is big
extern const int majorPieces[13]; // stores whether piece is major
extern const int minorPieces[13]; // stores whether piece is minor
extern const int piecePawn[13]; // stores whether piece is a pawn
extern const int pieceValues[13]; // stores piece value
extern const int pieceColours[13]; // stores piece colour

extern int filesBoard[BRD_SQ_NUM]; // holds the file index for a board position, 0 - 7 or 100 if offboard
extern int ranksBoard[BRD_SQ_NUM]; // holds the rank index for a board position, 0 - 7 or 100 if offboard

extern const int pieceKnight[13]; // used to ask 'is piece a knight?'
extern const int pieceBishopQueen[13]; // used to ask 'is piece a bishop or queen?'
extern const int pieceRookQueen[13]; // used to ask 'is piece a rook or queen?'
extern const int pieceKing[13]; // used to ask 'is piece a king?'
extern const int pieceSlides[13]; // used to ask 'is piece a sliding piece?' (queen/rook/bishop)

/* -- FUNCTIONS -- */
// init.cpp
extern void initAll();

// bitboards.cpp
extern void printBitBoard(U64 bitBoard);
extern int popBit(U64 *bitboard);
extern int countBits(U64 bitboard);

// hashkeys.cpp
extern U64 generatePositionKey(const Board *board);

// board.cpp
extern int checkBoard(const Board *board);
extern void updateMaterialLists(Board *board);
extern int parseFen(const char *fen, Board *board);
extern void resetBoard(Board *board);
extern void printBoard(const Board *board);

// attack.cpp
extern int isSquareAttacked(const int square, const int attackingSide, const Board *board);

// io.cpp
extern char *printSquare(const int square);
extern char *printMove(const int move);
extern void printMoveList(const MoveList *moveList);

// validate.cpp
extern bool squareOnBoard(const int sq);
extern bool sideValid(const int side);
extern bool fileOrRankValid(const int fileOrRank);
extern bool pieceValid(const int piece);
extern bool pieceValidOrEmpty(const int piece);

// movegen.cpp
extern void generateAllMoves(const Board *board, MoveList *list);

// makemove.cpp
extern int makeMove(Board *board, int move);
extern void takeMove(Board *board);

// perft.cpp
extern void perftTest(int depth, Board *board);

#endif //MAIOLICA_DEFS_H
