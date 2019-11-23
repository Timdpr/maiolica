#include "defs.h"

/// Populate Board's pvArray
int getPVLine(const int depth, Board *board) {
    ASSERT(depth < MAX_DEPTH)

    int move = probePVTable(board);
    int count = 0;

    // 'loop through board's pvArray until given depth, as long as legal moves are being found'
    while (move != NO_MOVE && count < depth) {
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

const int pvSize = 0x100000 * 2; // 2mb allocated

/// Loop through every entry in the table, setting each pos key and move to zero
void clearPVTable(PVTable *pvTable) {
    for (PVEntry *pvEntry = pvTable->pTable; pvEntry < pvTable->pTable + pvTable->numEntries; pvEntry++) {
        pvEntry->positionKey = U64(0);
        pvEntry->move = 0;
    }
}

/**
 * 'Initialise principal variation table'
 * by dynamically declaring the memory for what is essentially an array
 */
void initPVTable(PVTable *pvTable) {
    pvTable->numEntries = pvSize / sizeof(PVEntry); // how many entries can we fit inside 2mb?
    pvTable->numEntries -= 2; // make sure we don't go over!
    free(pvTable->pTable); // free table pointer's memory  // todo: this will crash for now, but gets changed later!
    pvTable->pTable = (PVEntry *) malloc(pvTable->numEntries * sizeof(PVEntry)); // allocate memory with similar logic to how it was freed!
    clearPVTable(pvTable);
    printf("pvTable init complete with %d entries\n", pvTable->numEntries);
}

/// Store a move in the table
void storePVMove(const Board *board, const int move) {
    // make index out of pos key modulo the number of entries!
    int index = board->positionKey % board->pvTable->numEntries;
    ASSERT(index >= 0 && index <= board->pvTable->numEntries-1)
    // add move and pos key to table at index
    board->pvTable->pTable[index].move = move;
    board->pvTable->pTable[index].positionKey = board->positionKey;
}

/// Get move indexed with this board's position key from principal variation table (if it exists)
int probePVTable(const Board *board) {
    // get index out of pos key modulo the number of entries!
    int index = board->positionKey % board->pvTable->numEntries;
    ASSERT(index >= 0 && index <= board->pvTable->numEntries-1)

    // if pos keys are equal, we can return move at the index
    if (board->pvTable->pTable[index].positionKey == board->positionKey) {
        return board->pvTable->pTable[index].move;
    }

    return NO_MOVE;
}
