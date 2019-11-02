#ifndef MAIOLICA_DEFS_H
#define MAIOLICA_DEFS_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define DEBUG

#ifndef DEBUG
#define ASSERT(n)
#else
#define ASSERT(n) \
if(!(n)) { \
std::printf("*** ASSERTION ERROR ***\n%s : FAILED ",#n); \
std::printf("on %s ",__DATE__); \
std::printf("at %s \n",__TIME__); \
std::printf("At line %d ",__LINE__); \
std::printf("in file %s \n",__FILE__); \
std::exit(0);}
#endif //DEBUG

#define NAME "Maiolica 1.0"
#define BRD_SQ_NUM 120
#define MAX_GAME_MOVES 2048
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
/// Holds game state at a particular ply
struct Undo {
    int move;
    int castlePermission;
    int enPas;
    int fiftyMove;
    U64 posKey;
};

struct Board {
    int pieces[BRD_SQ_NUM];
    U64 pawns[3]; // bitboard

    int kingSq[2];

    int side;
    int enPasSq; // en passant square, if set
    int fiftyMove;

    int ply;
    int plyHist;

    int castlingPerms;

    U64 positionKey; // hashkey

    int pieceNums[13]; // holds numbers of types pieces currently on the board (index corresponds to piece type enum)
    int bigPieces[2]; // # of non-pawn pieces by colour
    int majorPieces[2]; // # of rooks & queens by colour
    int minorPieces[2]; // # of bishops & knights by colour
    int material[2]; // total piece score for a side

    Undo history[MAX_GAME_MOVES]; // holds game state (via 'Undo' struct) at each ply

    int pieceList[13][10]; // 13 types of pieces, and at most 10 of each // TODO: May need to convert this to 1D for speed
};


/* -- MACROS -- */
/// Takes 64-based file and rank index and returns 120-based square index
#define FILE_RANK_TO_SQUARE_INDEX(file, rank) ( (21 + (file)) + ((rank) * 10) )
/// Takes 120-based square index and returns equivalent 64-based square index
#define INDEX_120_TO_64(sq120) (sq120ToSq64[(sq120)])
/// Takes 64-based square index and returns equivalent 120-based square index
#define INDEX_64_TO_120(sq64) (sq64ToSq120[(sq64)])
/// Finds first (least significant) bit in given bitboard, and returns index
#define POP(b) popBit(b)
/// Returns number of bits in bitboard
#define COUNT(b) countBits(b)
#define CLEAR_BIT(bitboard, sq) ((bitboard) &= clearMask[(sq)])
#define SET_BIT(bitboard, sq) ((bitboard) |= setMask[(sq)])
extern U64 RAND_64;


/* -- GLOBAL -- */  // TODO: Consider making these non-global...
extern int sq120ToSq64[BRD_SQ_NUM];
extern int sq64ToSq120[64];

extern U64 setMask[64];
extern U64 clearMask[64];
extern U64 pieceKeys[13][120];
extern U64 sideKey;
extern U64 castleKeys[16];

extern char pieceChars[];
extern char sideChars[];
extern char rankChars[];
extern char fileChars[];

extern int piecesBig[13]; // stores whether piece is big
extern int piecesMajor[13]; // stores whether piece is major
extern int piecesMinor[13]; // stores whether piece is minor
extern int piecesValue[13]; // stores piece value
extern int piecesColour[13]; // stores piece colour

extern int filesBoard[BRD_SQ_NUM]; // holds the file index for a board position, 0 - 7 or 100 if offboard
extern int ranksBoard[BRD_SQ_NUM]; // holds the rank index for a board position, 0 - 7 or 100 if offboard


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
extern int parseFen(char *fen, Board *board);
extern void resetBoard(Board *board);
extern void printBoard(const Board *board);


#endif //MAIOLICA_DEFS_H
