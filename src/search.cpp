#include "defs.h"

int isRepetition(const Board *board) {
    // start at current ply minus how long since the fifty move rule was reset, and look at hash keys
    for (int i = board->historyPly - board->fiftyMove; i < board->historyPly-1; ++i) { // todo: quicker way to do this in yt comments?
        ASSERT(i >= 0 && i < MAX_GAME_MOVES)
        // if current hash key matches, then this must be a repetition!
        if (board->positionKey == board->history[i].positionKey) {
            return TRUE;
        }
    }
    return FALSE;
}

void searchPosition(Board *board) {

}