#include "defs.h"

// Macros for extracting specific data out of the 'data' 64-bit int
#define EXTRACT_SCORE(x) ((x & 0xFFFF) - INF_BOUND)
#define EXTRACT_DEPTH(x) ((x >> 16) & 0x3F)
#define EXTRACT_FLAGS(x) ((x >> 23) & 0x3)
#define EXTRACT_MOVE(x) ((int)(x >> 25))

// Macro for combining data into the 'data' 64-bit int
#define FOLD_DATA(score, depth, flags, move) ( (score + INF_BOUND) | (depth << 16) | (flags << 23) | ((U64)move << 25))

void dataCheck(const int move) {
    int depth = rand() % MAX_DEPTH;
    int flags = rand() % 3;
    int score = rand() % ALPHABETA_BOUND;

    U64 data = FOLD_DATA(score, depth, flags, move);

    printf("Orig : move:%s d:%d fl:%d sc:%d data:%llX\n", printMove(move), depth, flags, score, data);
    printf("Check: move:%s d:%d fl:%d sc:%d\n\n",
           printMove(EXTRACT_MOVE(data)),
           EXTRACT_DEPTH(data),
           EXTRACT_FLAGS(data),
           EXTRACT_SCORE(data));
}

void tempHashTest(char *fen) {
    Board board{};
    parseFen(fen, board);

    MoveList moveList[1];
    generateAllMoves(board, moveList);

    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        if (!makeMove(board, moveList->moves[moveNum].move)) {
            continue;
        }
        undoMove(board);
        dataCheck(moveList->moves[moveNum].move);
    }


}

HashTable hashTable[1];

/// Populate Board's pvArray
int getPVLine(const int depth, Board& board, const HashTable *table) {
    ASSERT(depth < MAX_DEPTH && depth >= 1)

    int move = probePVTable(board, table);
    int count = 0;

    // 'loop through board's pvArray until given depth, as long as legal moves are being found'
    while (move != NO_MOVE && count < depth) {
        ASSERT(count < MAX_DEPTH)
        if (moveExists(board, move)) {
            makeMove(board, move);
            board.pvArray[count++] = move; // populate pvArray with move
        } else {
            break;
        }
        move = probePVTable(board, table);
    }

    // Take the moves back!!!!11!!1!
    while (board.ply > 0) {
        undoMove(board);
    }

    return count;
}

/// Loop through every entry in the table, setting each pos key and move to zero
void clearHashTable(HashTable *table) {
    for (HashEntry *hashEntry = table->hTable; hashEntry < table->hTable + table->numEntries; hashEntry++) {
        hashEntry->positionKey = U64(0);
        hashEntry->move = NO_MOVE;
        hashEntry->depth = 0;
        hashEntry->score = 0;
        hashEntry->flags = 0;
        hashEntry->age = 0;
    }
    table->currentAge = 0;
}

/**
 * 'Initialise hash table'
 * by dynamically declaring the memory for what is essentially an array
 */
void initHashTable(HashTable *table, const int MB) {
    int hashSize = 0x100000 * MB;
    table->numEntries = hashSize / sizeof(HashEntry); // how many entries can we fit inside?
    table->numEntries -= 2; // make sure we don't go over!

    if (table->hTable != nullptr) {
        free(table->hTable); // free table pointer's memory
    }
    // Todo: May have to change this substantially for larger sizes...
    table->hTable = (HashEntry *) malloc(table->numEntries * sizeof(HashEntry)); // allocate memory with similar logic to how it was freed!
    if (table->hTable == nullptr) {
        printf("Hash table allocation failed, trying again with %dMB", MB/2);
        initHashTable(table, MB / 2);
    } else {
        clearHashTable(table);
        printf("HashTable init complete with %d entries\n", table->numEntries);
    }
}

/// Get move indexed with this board's position key from principal variation table (if it exists)
int probeHashEntry(Board& board, HashTable *table, int *move, int *score, int alpha, int beta, int depth) {
    // get index out of pos key modulo the number of entries!
    int index = board.positionKey % table->numEntries;

    ASSERT(index >= 0 && index <= hashTable->numEntries - 1)
    ASSERT(depth >=1 && depth < MAX_DEPTH)
    ASSERT(alpha < beta)
    ASSERT(alpha >= -ALPHABETA_BOUND && alpha <= ALPHABETA_BOUND)
    ASSERT(beta >= -ALPHABETA_BOUND && beta <= ALPHABETA_BOUND)
    ASSERT(board.ply >= 0 && board.ply < MAX_DEPTH)

    // if pos keys are equal...
    if (table->hTable[index].positionKey == board.positionKey) {
        *move = table->hTable[index].move; // get move
        if (table->hTable[index].depth >= depth) { // if depth >= search depth
            table->hit++; // we have a hit!

            ASSERT(hashTable->hTable[index].depth >= 1 && hashTable->hTable[index].depth < MAX_DEPTH)
            ASSERT(hashTable->hTable[index].flags >= HF_ALPHA && hashTable->hTable[index].flags <= HF_EXACT)

            // get score & adjust according to ply if it is a mate score (we got rid of this when storing)
            *score = table->hTable[index].score;
            if (*score > IS_MATE) {
                *score -= board.ply;
            } else if (*score < -IS_MATE) {
                *score += board.ply;
            }

            // check for match depending on flags
            switch (table->hTable[index].flags) {
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
    return NO_MOVE;
}

/// Store a move in the table
/// TODO: Look up the more complex algorithms for how and when to store hash entries
void storeHashEntry(Board& board, HashTable *table, const int move, int score, const int flags, const int depth) {
    // make index out of pos key modulo the number of entries!
    int index = board.positionKey % table->numEntries;

    ASSERT(index >= 0 && index <= hashTable->numEntries - 1)
    ASSERT(depth >= 1 && depth < MAX_DEPTH)
    ASSERT(flags >= HF_ALPHA && flags <= HF_EXACT)
    ASSERT(score >= -INF_BOUND && score <= INF_BOUND)
    ASSERT(board.ply >= 0 && board.ply < MAX_DEPTH)

    bool replace = false;
    if (table->hTable[index].positionKey == 0) { // new entry
        replace = true;
    } else { // otherwise, if index that we're trying to replace is older than our current entry, or the depth is greater, then we replace
        if (table->hTable[index].age < table->currentAge || table->hTable[index].depth <= depth) {
            replace = true;
        }
    }
    if (!replace) {
        return; // return early without storing the worse entry!
    }

    // reset score back to 'infinite' (after it was adjusted by ply in the search)
    if (score > IS_MATE) {
        score += board.ply;
    } else if (score < -IS_MATE) {
        score -= board.ply;
    }

    // add move and pos key to table at index
    table->hTable[index].move = move;
    table->hTable[index].positionKey = board.positionKey;
    table->hTable[index].flags = flags;
    table->hTable[index].score = score;
    table->hTable[index].depth = depth;
    table->hTable[index].age = table->currentAge;
}

int probePVTable(const Board& board, const HashTable *table) {
    // get index out of pos key modulo the number of entries!
    int index = board.positionKey % table->numEntries;
    ASSERT(index >= 0 && index <= hashTable->numEntries - 1)

    // if pos keys are equal, we can return move at the index
    if (table->hTable[index].positionKey == board.positionKey) {
        return table->hTable[index].move;
    }
    return NO_MOVE;
}
