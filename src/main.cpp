#include <cstring>
#include "defs.h"

int main(int argc, char *argv[]) {
    initAll();
    initHashTable(hashTable, 64);

    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);

    // './maiolica bench' runs the benchmark and exits
    if (argc > 1 && !strcmp(argv[1], "bench")) {
        bench();
        free(hashTable->hTable);
        return 0;
    }

    std::printf("%s by Tim Russell\n", NAME);
    uciLoop();

    free(hashTable->hTable);
    return 0;
}
