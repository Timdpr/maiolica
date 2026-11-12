#ifndef MAIOLICA_DEFS_H
#define MAIOLICA_DEFS_H

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <type_traits>

#ifdef DEBUG
// prints to stderr and aborts, so a failure can't look like success to a test runner
#define ASSERT(n) \
if(!(n)) { \
std::fprintf(stderr, "\n*** ASSERTION ERROR ***\n%s : FAILED ",#n); \
std::fprintf(stderr, "At line %d ",__LINE__); \
std::fprintf(stderr, "in file %s \n",__FILE__); \
std::abort();}
#else
#define ASSERT(n)
#endif

constexpr const char *NAME = "Maiolica 1.0";
constexpr int BRD_SQ_NUM = 120; // number of squares on the main, 'mailbox representation' board
constexpr int MAX_GAME_MOVES = 2048;
constexpr int MAX_POSITION_MOVES = 256;
constexpr int MAX_DEPTH = 64;
constexpr const char *START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr int INF_BOUND = 32000;
constexpr int ALPHABETA_BOUND = 30000;
constexpr int IS_MATE = ALPHABETA_BOUND - MAX_DEPTH;

using U64 = std::uint64_t;
using TimeMS = std::chrono::milliseconds::rep;


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

enum { wK_CA = 1, wQ_CA = 2, bK_CA = 4, bQ_CA = 8 }; // Castling permissions

enum { HF_NONE, HF_ALPHA, HF_BETA, HF_EXACT };


/* -- STRUCTS -- */

/** Uses an integer to store all necessary information, and another int for the score

0000 0000 0000 0000 0000 0111 1111 -> 'From' square: 0x7F                         <br>
0000 0000 0000 0011 1111 1000 0000 -> 'To' square: >>7, 0x7F                      <br>
0000 0000 0011 1100 0000 0000 0000 -> Piece captured, if any: >>14, 0xF           <br>
0000 0000 0100 0000 0000 0000 0000 -> En passant?: bit and, 0x40000               <br>
0000 0000 1000 0000 0000 0000 0000 -> Pawn start move?: bit and, 0x80000          <br>
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

/// Transposition table entry
struct HashEntry {
    U64 positionKey;
    int move;
    int score;
    int depth;
    int flags;
    int age;
};

/// Transposition table
struct HashTable {
    HashEntry *hTable; // pointer to element
    int numEntries;
    int hit;
    int cut;
    int currentAge;
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

    int pieceList[13][10]; // 13 types of pieces, and at most 10 of each

    int pvArray[MAX_DEPTH];

    int searchHistory[13][BRD_SQ_NUM]; // indexed by piece type and board square
    int searchKillers[2][MAX_DEPTH]; // indexed by 2 killer moves, by depth
};
static_assert(std::is_trivially_copyable_v<Board>); // boards get copied whole: for the search thread, & by the gui code

/// Limits & stats for a search. The atomics are shared between the search & input threads
struct SearchInfo {
    TimeMS startTime;
    std::atomic<TimeMS> stopTime{0};
    std::atomic<bool> timeSet{false}; // only stop on time if this is set
    int depth;
    long nodeLimit; // 'go nodes', 0 if none
    bool infinite; // 'go infinite': don't send bestmove until told to stop

    long nodes; // count of all positions the engine visits in the search tree
    int selDepth; // deepest ply reached

    std::atomic<bool> stopped{false};
    std::atomic<bool> ponder{false}; // pondering: no time limit & no bestmove until 'ponderhit' or 'stop'

    float failHigh; // divide fhf by fh to get an idea of how good move ordering is
    float failHighFirst; // try to make it >90%
    int nullCut;
};


/* -- MACROS -- */

// - Square index macros:
/// Takes 64-based file and rank index and returns 120-based square index
#define FILE_RANK_TO_SQUARE_INDEX(file, rank) ( (21 + (file)) + ((rank) * 10) )
/// Takes 120-based square index and returns equivalent 64-based square index
#define INDEX_120_TO_64(sq120) (sq120ToSq64[(sq120)])
/// Takes 64-based square index and returns equivalent 120-based square index
#define INDEX_64_TO_120(sq64) (sq64ToSq120[(sq64)])

// - Bitboard macros:
/// Clear the bit for the given (64-based) square
#define CLEAR_BIT(bitboard, sq) ((bitboard) &= ~(U64(1) << (sq)))
/// Set the bit for the given (64-based) square
#define SET_BIT(bitboard, sq) ((bitboard) |= (U64(1) << (sq)))

// - Piece checking macros:
#define IS_KNIGHT(piece) (pieceKnight[(piece)])
#define IS_BISHOP_OR_QUEEN(piece) (pieceBishopQueen[(piece)])
#define IS_ROOK_OR_QUEEN(piece) (pieceRookQueen[(piece)])
#define IS_KING(piece) (pieceKing[(piece)])

/// Get square index for black from white, as eval tables are designed for white
#define MIRROR_64(square) (mirror64[(square)])

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
constexpr int MFLAG_EN_PASSANT = 0x40000;
/// Set pawn start flag in move by ORing in this flag
constexpr int MFLAG_PAWN_START = 0x80000;
/// Set castling flag in move by ORing in this flag
constexpr int MFLAG_CASTLING = 0x1000000;
/// AND with this mask to see if a move captures (a captured piece, or en passant)
constexpr int MFLAG_CAPTURING = 0x7C000;
/// AND with this mask to see if a move promotes
constexpr int MFLAG_PROMOTION = 0xF00000;

constexpr int NO_MOVE = 0;

/* -- GLOBAL -- */  // TODO: Consider making these non-global...

extern int sq120ToSq64[BRD_SQ_NUM];
extern int sq64ToSq120[64];

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

extern const int mirror64[64];

extern U64 fileBitMask[8]; // all '1's for the indexed file
extern U64 rankBitMask[8]; // all '1's for the indexed rank

extern U64 whitePassedMask[64]; // does the indexed white pawn have a 'clear run' to promotion?
extern U64 blackPassedMask[64]; // does the indexed black pawn have a 'clear run' to promotion?
extern U64 isolatedPawnMask[64]; // is the indexed pawn 'isolated'?

extern HashTable hashTable[1]; // initialise hash table to size 1 array, but this will be dynamically allocated

/* -- FUNCTIONS -- */

// init.cpp
void initAll();

// bitboards.cpp
void printBitBoard(U64 bitBoard);
int popBit(U64 *bitboard);
int countBits(U64 bitboard);

// hashkeys.cpp
U64 generatePositionKey(const Board& board);

// board.cpp
int checkBoard(const Board& board);
void updateMaterialLists(Board& board);
int parseFen(const char *fen, Board& board);
void resetBoard(Board& board);
void printBoard(const Board& board);
void mirrorBoard(Board& board);

// attack.cpp
int isSquareAttacked(int square, int attackingSide, const Board& board);

// io.cpp
char *printSquare(int square);
char *printMove(int move);
void printMoveList(const MoveList *moveList);
[[nodiscard]] int parseMove(const char *ptrChar, Board& board);

// validate.cpp
int moveListValid(const MoveList *list, const Board& board);
int squareIs120(int sq);
int squareOnBoard(int sq);
int sideValid(int side);
int fileOrRankValid(int fileOrRank);
int pieceValid(int piece);
int pieceValidEmpty(int piece);
int pieceValidEmptyOffboard(int pce);
void debugAnalysisTest(const char *epdFile, Board& board, SearchInfo *info, HashTable *table);
void mirrorEvalTest(Board& board);

// movegen.cpp
void generateAllMoves(const Board& board, MoveList *list);
void generateAllCaptureMoves(const Board& board, MoveList *moveList);
[[nodiscard]] int moveExists(Board& board, int move);
void initMVVLVA();

// makemove.cpp
[[nodiscard]] int makeMove(Board& board, int move);
void undoMove(Board& board);
void makeNullMove(Board& board);
void undoNullMove(Board& board);

// perft.cpp
extern long leafNodes;
void perft(int depth, Board& board);
void perftTest(int depth, Board& board);

// misc.cpp
TimeMS getTimeMS();

// search.cpp
void searchPosition(Board& board, SearchInfo *info, HashTable *table);

// pvtable.cpp
void clearHashTable(HashTable *table);
int getPVLine(int depth, Board& board, const HashTable *table);
void initHashTable(HashTable *table, int MB);
void storeHashEntry(Board& board, HashTable *table, int move, int score, int flags, int depth);
[[nodiscard]] int probeHashEntry(Board& board, HashTable *table, int *move, int *score, int alpha, int beta, int depth);
int probePVTable(const Board& board, const HashTable *table);
int hashFull(const HashTable *table);

// evaluate.cpp
int evalPosition(const Board& board);

// uci.cpp
void uciLoop();
void sendLine(const char *format, ...);
extern bool debugMode;
extern void (*lineHandler)(const char *line);

// console.cpp
void consoleLoop();
void replayPgn(const char *file, int delay);

// bench.cpp
void bench();

#endif //MAIOLICA_DEFS_H
