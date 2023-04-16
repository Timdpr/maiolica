#include <cstring>
#include "defs.h"

#define WAC1 "r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 1"
#define PERFT "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"

int main() {
    // TODO: see part 87 for next steps...
    // TODO: Not doing polyglot books for now
    initAll();

    Board board{};
    SearchInfo info[1];
    info->quit = false;
    initHashTable(hashTable, 64);

    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);

//    tempHashTest(PERFT);
//    tempHashTest(WAC1);
//    exit(0);

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

    free(hashTable->hTable);

    return 0;
}
