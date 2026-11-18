#include "defs.h"

HashTable hashTable;

// Where each field sits in an entry's data word. The move is the low 25 bits, as it is everywhere else
constexpr int TT_SCORE_SHIFT = 25, TT_DEPTH_SHIFT = 41, TT_FLAGS_SHIFT = 48, TT_AGE_SHIFT = 50;
constexpr int TT_MOVE_MASK = 0x1FFFFFF, TT_SCORE_MASK = 0xFFFF, TT_DEPTH_MASK = 0x7F, TT_FLAGS_MASK = 0x3, TT_AGE_MASK = 0x3FFF;
constexpr int TT_SCORE_OFFSET = 32768; // scores are stored from 0 up
static_assert(INF_BOUND + MAX_DEPTH < TT_SCORE_OFFSET && MAX_DEPTH <= TT_DEPTH_MASK && HF_EXACT <= TT_FLAGS_MASK);

static U64 packEntry(int move, int score, int depth, int flags, int age) {
    return U64(move) | (U64(score + TT_SCORE_OFFSET) << TT_SCORE_SHIFT) | (U64(depth) << TT_DEPTH_SHIFT)
           | (U64(flags) << TT_FLAGS_SHIFT) | (U64(age) << TT_AGE_SHIFT);
}
static int entryMove(U64 data) { return int(data & TT_MOVE_MASK); }
static int entryScore(U64 data) { return int((data >> TT_SCORE_SHIFT) & TT_SCORE_MASK) - TT_SCORE_OFFSET; }
static int entryDepth(U64 data) { return int((data >> TT_DEPTH_SHIFT) & TT_DEPTH_MASK); }
static int entryFlags(U64 data) { return int((data >> TT_FLAGS_SHIFT) & TT_FLAGS_MASK); }
static int entryAge(U64 data) { return int(data >> TT_AGE_SHIFT); }

/// The table's age as an entry holds it. It wraps after 16,384 searches without a new game
static int tableAge(const HashTable& table) {
    return table.currentAge & TT_AGE_MASK;
}

/// Read an entry's two words once each, & hand back the data if it is this key's. 0 if not: no entry's data is 0.
/// Everything is then taken from that one copy: read the table again & another write may have landed in between
static U64 readEntry(const HashTable& table, int index, U64 positionKey) {
    const HashEntry& entry = table.hTable[index];
    U64 data = entry.data.load(std::memory_order_relaxed);
    return (entry.keyXorData.load(std::memory_order_relaxed) ^ data) == positionKey ? data : 0;
}

/// Fill pvArray with the line from this position: firstMove, then the table's moves. It ends with NO_MOVE.
/// Returns its length. The first move is the search's own & not the table's, as the entry for the position
/// searched isn't always the one this search stored.
/// It walks a copy: making & undoing moves on the search's own board reorders its piece lists,
/// and then how far the line went would change what gets searched next
int getPVLine(const int depth, const Board& board, const HashTable& table, const int firstMove, int *pvArray) {
    ASSERT(depth < MAX_DEPTH && depth >= 1)

    Board walk = board;
    int move = firstMove;
    int count = 0;

    // 'loop through the table's moves until given depth, as long as legal moves are being found'
    while (move != NO_MOVE && count < depth) {
        ASSERT(count < MAX_DEPTH)
        if (moveExists(walk, move) && makeMove(walk, move)) { // it's legal, so it gets made
            pvArray[count++] = move; // populate pvArray with move
        } else {
            break;
        }
        // the search calls a repetition a draw & looks no further, so the pv ends with this move
        if (isRepetition(walk) || walk.fiftyMove >= 100) {
            break;
        }
        move = probePVTable(walk, table);
    }
    pvArray[count] = NO_MOVE;

    return count;
}

/// The table's reply to a move, for something to ponder on when the pv is only that move long
/// (a repetition ends the pv, but not the game). NO_MOVE if the table has no legal reply
int probePonderMove(const Board& board, const int move, const HashTable& table) {
    Board next = board;
    if (!makeMove(next, move)) {
        return NO_MOVE;
    }
    int reply = probePVTable(next, table);
    return (reply != NO_MOVE && moveExists(next, reply)) ? reply : NO_MOVE;
}

/// Loop through every entry in the table, setting each to zero, which is what an empty one looks like
void clearHashTable(HashTable& table) {
    for (HashEntry *hashEntry = table.hTable; hashEntry < table.hTable + table.numEntries; hashEntry++) {
        hashEntry->keyXorData.store(0, std::memory_order_relaxed);
        hashEntry->data.store(0, std::memory_order_relaxed);
    }
    table.currentAge = 0;
}

/**
 * 'Initialise hash table'
 * by dynamically declaring the memory for what is essentially an array
 */
void initHashTable(HashTable& table, const int MB) {
    size_t hashSize = 0x100000 * size_t(MB); // size_t, as an int overflows at 2048MB
    table.numEntries = int(hashSize / (2 * sizeof(HashEntry))); // half of what would fit, which is how many there were at 32 bytes each
    table.numEntries -= 2; // make sure we don't go over!

    if (table.hTable != nullptr) {
        free(table.hTable); // free table pointer's memory
    }
    table.hTable = (HashEntry *) malloc(size_t(table.numEntries) * sizeof(HashEntry)); // allocate memory with similar logic to how it was freed!
    if (table.hTable == nullptr && MB > 1) {
        sendLine("info string hash allocation failed, trying %dMB", MB / 2);
        initHashTable(table, MB / 2);
    } else if (table.hTable != nullptr) {
        clearHashTable(table);
    }
}

/// How full the table is, in permill, sampled from the first 1000 entries (for 'info hashfull')
int hashFull(const HashTable& table) {
    int used = 0;
    for (int i = 0; i < 1000 && i < table.numEntries; ++i) {
        U64 data = table.hTable[i].data.load(std::memory_order_relaxed);
        used += data != 0 && entryAge(data) == tableAge(table);
    }
    return used;
}

/// Get move indexed with this board's position key from principal variation table (if it exists)
int probeHashEntry(Board& board, HashTable& table, int *move, int *score, int alpha, int beta, int depth) {
    // get index out of pos key modulo the number of entries!
    int index = board.positionKey % table.numEntries;

    ASSERT(index >= 0 && index <= hashTable.numEntries - 1)
    ASSERT(depth >=1 && depth < MAX_DEPTH)
    ASSERT(alpha < beta)
    ASSERT(alpha >= -ALPHABETA_BOUND && alpha <= ALPHABETA_BOUND)
    ASSERT(beta >= -ALPHABETA_BOUND && beta <= ALPHABETA_BOUND)
    ASSERT(board.ply >= 0 && board.ply < MAX_DEPTH)

    // if pos keys are equal...
    U64 data = readEntry(table, index, board.positionKey);
    if (data != 0) {
        *move = entryMove(data); // get move
        if (entryDepth(data) >= depth) { // if depth >= search depth
            ASSERT(entryDepth(data) >= 1 && entryDepth(data) < MAX_DEPTH)
            ASSERT(entryFlags(data) >= HF_ALPHA && entryFlags(data) <= HF_EXACT)

            // get score & adjust according to ply if it is a mate score (we got rid of this when storing)
            *score = entryScore(data);
            if (*score > IS_MATE) {
                *score -= board.ply;
            } else if (*score < -IS_MATE) {
                *score += board.ply;
            }

            // check for match depending on flags
            switch (entryFlags(data)) {
                case HF_ALPHA:
                    if (*score <= alpha) {
                        *score = alpha;
                        return true;
                    }
                    break;
                case HF_BETA:
                    if (*score >= beta) {
                        *score = beta;
                        return true;
                    }
                    break;
                case HF_EXACT:
                    return true;
                default: ASSERT(false) break;
            }
        }
    }
    return false;
}

/// Store a move in the table
/// TODO: Look up the more complex algorithms for how and when to store hash entries
void storeHashEntry(Board& board, HashTable& table, const int move, int score, const int flags, const int depth) {
    // make index out of pos key modulo the number of entries!
    int index = board.positionKey % table.numEntries;

    ASSERT(index >= 0 && index <= hashTable.numEntries - 1)
    ASSERT(depth >= 1 && depth < MAX_DEPTH)
    ASSERT(flags >= HF_ALPHA && flags <= HF_EXACT)
    ASSERT(score >= -INF_BOUND && score <= INF_BOUND)
    ASSERT(board.ply >= 0 && board.ply < MAX_DEPTH)

    // an empty slot is always taken. So is an entry from an earlier search, or one searched no deeper than this
    HashEntry& entry = table.hTable[index];
    U64 old = entry.data.load(std::memory_order_relaxed);
    if (old != 0 && entryAge(old) == tableAge(table) && entryDepth(old) > depth) {
        return; // return early without storing the worse entry!
    }

    // reset score back to 'infinite' (after it was adjusted by ply in the search)
    if (score > IS_MATE) {
        score += board.ply;
    } else if (score < -IS_MATE) {
        score -= board.ply;
    }

    // pack it all into the data word, & store that and the pos key xored with it
    U64 data = packEntry(move, score, depth, flags, tableAge(table));
    ASSERT(entryMove(data) == move && entryScore(data) == score && entryDepth(data) == depth && entryFlags(data) == flags)
    entry.keyXorData.store(board.positionKey ^ data, std::memory_order_relaxed);
    entry.data.store(data, std::memory_order_relaxed);
}

int probePVTable(const Board& board, const HashTable& table) {
    // get index out of pos key modulo the number of entries!
    int index = board.positionKey % table.numEntries;
    ASSERT(index >= 0 && index <= hashTable.numEntries - 1)

    // if pos keys are equal, we can return move at the index (& NO_MOVE is 0, as is the data when they aren't)
    return entryMove(readEntry(table, index, board.positionKey));
}
