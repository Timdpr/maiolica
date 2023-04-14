#include <cstring>
#include "defs.h"

int main() {
    // TODO: see part 87 for next steps...
    // TODO: Not doing polyglot books for now
    initAll();

    Board board = genBoard();
    SearchInfo info[1];
    info->quit = false;
    initHashTable(board.hashTable, 64);

    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);

    printf("Welcome to Maiolica! Type 'console' for console mode...\n");

    char line[256];
    while (true) {
        memset(&line[0], 0, sizeof(line));
        fflush(stdout);

        if (!fgets(line, 256, stdin))
            continue;

        if (line[0] == '\n')
            continue;

        if (!strncmp(line, "uci", 3)) {
            uciLoop(board, info);
            if (info->quit == true) break;
            continue;

        } else if (!strncmp(line, "quit", 4))	{
            break;
        }
    }

    free(board.hashTable->hTable);

    return 0;
}
