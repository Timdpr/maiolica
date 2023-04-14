#include "defs.h"

/// Populate Board's pvArray
int getPVLine(const int depth, Board *board) {
    ASSERT(depth < MAX_DEPTH && depth >= 1)

    int move = probePVTable(board);
    int count = 0;

    // 'loop through board's pvArray until given depth, as long as legal moves are being found'
    while (move != NO_MOVE && count < depth) {
        ASSERT(count < MAX_DEPTH)
        if (moveExists(board, move)) {
            makeMove(board, move);
            board->pvArray[count++] = move; // populate pvArray with move
        } else {
            break;
        }
        move = probePVTable(board);
    }

    // Take the moves back!!!!11!!1!
    while (board->ply > 0) {
        takeMove(board);
    }

    return count;
}

/// Loop through every entry in the table, setting each pos key and move to zero
void clearHashTable(HashTable *hashTable) {
    for (HashEntry *hashEntry = hashTable->hTable; hashEntry < hashTable->hTable + hashTable->numEntries; hashEntry++) {
        hashEntry->positionKey = U64(0);
        hashEntry->move = NO_MOVE;
        hashEntry->depth = 0;
        hashEntry->score = 0;
        hashEntry->flags = 0;
    }
    hashTable->newWrite = 0;
}

/**
 * 'Initialise hash table'
 * by dynamically declaring the memory for what is essentially an array
 */
void initHashTable(HashTable *hashTable, const int MB) {
    int hashSize = 0x100000 * MB;
    hashTable->numEntries = hashSize / sizeof(HashEntry); // how many entries can we fit inside?
    hashTable->numEntries -= 2; // make sure we don't go over!

    if (hashTable->hTable != nullptr) {
        free(hashTable->hTable); // free table pointer's memory
    }
    // Todo: May have to change this substantially for larger sizes...
    hashTable->hTable = (HashEntry *) malloc(hashTable->numEntries * sizeof(HashEntry)); // allocate memory with similar logic to how it was freed!
    if (hashTable->hTable == nullptr) {
        printf("Hash table allocation failed, trying again with %dMB", MB/2);
        initHashTable(hashTable, MB / 2);
    } else {
        clearHashTable(hashTable);
        printf("HashTable init complete with %d entries\n", hashTable->numEntries);
    }
}

/// Get move indexed with this board's position key from principal variation table (if it exists)
int probeHashEntry(Board *board, int *move, int *score, int alpha, int beta, int depth) {
    // get index out of pos key modulo the number of entries!
    int index = board->positionKey % board->hashTable->numEntries;

    ASSERT(index >= 0 && index <= board->hashTable->numEntries - 1)
    ASSERT(depth >=1 && depth < MAX_DEPTH)
    ASSERT(alpha < beta)
    ASSERT(alpha >= -INFINITE && alpha <= INFINITE)
    ASSERT(beta >= -INFINITE && beta <= INFINITE)
    ASSERT(board->ply >= 0 && board->ply < MAX_DEPTH)

    // if pos keys are equal...
    if (board->hashTable->hTable[index].positionKey == board->positionKey) {
        *move = board->hashTable->hTable[index].move; // get move
        if (board->hashTable->hTable[index].depth >= depth) { // if depth >= search depth
            board->hashTable->hit++; // we have a hit!

            ASSERT(board->hashTable->hTable[index].depth >= 1 && board->hashTable->hTable[index].depth < MAX_DEPTH)
            ASSERT(board->hashTable->hTable[index].flags >= HF_ALPHA && board->hashTable->hTable[index].flags <= HF_EXACT)

            // get score & adjust according to ply if it is a mate score (we got rid of this when storing)
            *score = board->hashTable->hTable[index].score;
            if (*score > IS_MATE) {
                *score -= board->ply;
            } else if (*score < -IS_MATE) {
                *score += board->ply;
            }

            // check for match depending on flags
            switch (board->hashTable->hTable[index].flags) {
                ASSERT(*score >= -INFINITE && *score <= INFINITE)
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
void storeHashEntry(Board *board, const int move, int score, const int flags, const int depth) {
    // make index out of pos key modulo the number of entries!
    int index = board->positionKey % board->hashTable->numEntries;

    ASSERT(index >= 0 && index <= board->hashTable->numEntries - 1)
    ASSERT(depth >= 1 && depth < MAX_DEPTH)
    ASSERT(flags >= HF_ALPHA && flags <= HF_EXACT)
    ASSERT(score >= -INFINITE && score <= INFINITE)
    ASSERT(board->ply >= 0 && board->ply < MAX_DEPTH)

    // keep count of new entries vs overwriting entries
    if (board->hashTable->hTable[index].positionKey == 0) {
        board->hashTable->newWrite++;
    } else {
        board->hashTable->overWrite++;
    }

    // reset score back to 'infinite' (after it was adjusted by ply in the search)
    if (score > IS_MATE) {
        score += board->ply;
    } else if (score < -IS_MATE) {
        score -= board->ply;
    }

    // add move and pos key to table at index
    board->hashTable->hTable[index].move = move;
    board->hashTable->hTable[index].positionKey = board->positionKey;
    board->hashTable->hTable[index].flags = flags;
    board->hashTable->hTable[index].score = score;
    board->hashTable->hTable[index].depth = depth;
}

int probePVTable(const Board *board) {
    // get index out of pos key modulo the number of entries!
    int index = board->positionKey % board->hashTable->numEntries;
    ASSERT(index >= 0 && index <= board->hashTable->numEntries - 1)

    // if pos keys are equal, we can return move at the index
    if (board->hashTable->hTable[index].positionKey == board->positionKey) {
        return board->hashTable->hTable[index].move;
    }
    return NO_MOVE;
}
