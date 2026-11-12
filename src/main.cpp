#include <cstring>
#include "defs.h"

int main(int argc, char *argv[]) {
    initAll();
    initHashTable(hashTable, 64);

    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);

    // './maiolica bench' runs the benchmark, 'console' plays in the terminal, 'replay <pgn> [ms]' shows games
    if (argc > 1 && !strcmp(argv[1], "bench")) {
        bench();
    } else if (argc > 1 && !strcmp(argv[1], "console")) {
        consoleLoop();
    } else if (argc > 2 && !strcmp(argv[1], "replay")) {
        replayPgn(argv[2], argc > 3 ? atoi(argv[3]) : 800);
    } else {
        std::printf("%s by Tim Russell\n", NAME);
        uciLoop();
    }

    free(hashTable->hTable);
    return 0;
}
