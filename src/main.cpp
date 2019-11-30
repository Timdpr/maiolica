#include "defs.h"

int main() {
    // TODO: videos 72, 73, 74 AND 75!!
    initAll();

    Board *board = genBoard();
    SearchInfo info[1];
    initPVTable(board->pvTable);
    info->quit = FALSE;

    uciLoop(board, info);

    free(board->pvTable->pTable);

    return 0;
}
