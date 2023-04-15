#include "defs.h"
#include <cstdio>
#include <cstring>

/// This is what we expect the largest input to be:
#define INPUT_BUFFER (400 * 6)

int fastAtoi(const char* str) {
    int val = 0;
    uint8_t x;
    while ((x = uint8_t(*str++ - '0')) <= 9) val = val * 10 + x;
    return val;
}

void parseGo(const char* line, SearchInfo *info, Board& board) {
    // e.g. go depth 6 wtime 180000 btime 100000 binc 1000 winc 1000 movetime 1000 movestogo 40
    int depth = -1;
    int movesToGo = 30; // defaulting to sudden death time control
    int moveTime = -1;
    int time = -1;
    int increment = 0;

    const char *ptr = nullptr;
    info->timeSet = false;

    if ((ptr = strstr(line, "infinite"))) {

    }

    if ((ptr = strstr(line,"binc")) && board.side == BLACK) {
        increment = fastAtoi(ptr + 5); // atoi converts string to integer
    }

    if ((ptr = strstr(line,"winc")) && board.side == WHITE) {
        increment = fastAtoi(ptr + 5);
    }

    if ((ptr = strstr(line,"wtime")) && board.side == WHITE) {
        time = fastAtoi(ptr + 6);
    }

    if ((ptr = strstr(line,"btime")) && board.side == BLACK) {
        time = fastAtoi(ptr + 6);
    }

    if ((ptr = strstr(line,"movestogo"))) {
        movesToGo = fastAtoi(ptr + 10);
    }

    if ((ptr = strstr(line,"movetime"))) {
        moveTime = fastAtoi(ptr + 9);
    }

    if ((ptr = strstr(line, "depth"))) {
        depth = fastAtoi(ptr + 6);
    }


    if (moveTime != -1) {
        time = moveTime;
        movesToGo = 1; // hack! we later divide time by movesToGo, so this needs to be 1 to just give the time
    }

    info->startTime = getTimeMS();
    info->depth = depth;

    if (time != -1) {
        info->timeSet = true;
        time /= movesToGo;
        time -= 50; // take 50ms off for safety!
        info->stopTime = info->startTime + time + increment;
    }

    if (depth == -1) {
        info->depth = MAX_DEPTH;
    }

    std::printf("time:%d start:%lld stop:%lld depth:%d timeset:%d\n",
           time, info->startTime, info->stopTime, info->depth, info->timeSet);

    searchPosition(board, info, hashTable); // print best move to the gui
}

/// reads a fen or 'startpos', possibly followed by moves
void parsePosition(const char* lineIn, Board& board) {
    lineIn += 9; // move pointer forward 9 chars
    const char *ptrChar = lineIn;

    // if startpos, parse start fen
    if (strncmp(lineIn, "startpos", 8) == 0) {
        parseFen(START_FEN, board);
    } else {
        // check if 'fen' is in the string
        ptrChar = strstr(lineIn, "fen");
        if (ptrChar == nullptr) {
            parseFen(START_FEN, board); // if it isn't, parse the start fen
        } else {
            ptrChar += 4;
            parseFen(ptrChar, board); // if there is a fen, parse it!
        }
    }

    // move pointer character to 'moves'
    ptrChar = strstr(lineIn, "moves");
    if (ptrChar != nullptr) { // if there are moves
        ptrChar += 6; // move pointer to first move
        while (*ptrChar) { // while we are pointing to something
            int move = parseMove(ptrChar, board); // parse the move, exiting if something is wrong
            if (move == NO_MOVE) {
                break;
            }
            makeMove(board, move); // make the move
            board.ply = 0; // set ply back to 0, since it gets incremented to 1 in makeMove
            while (*ptrChar && *ptrChar != ' ') { // while we are pointing to something AND haven't found a space
                ptrChar++; // move pointer through the move
            }
            ptrChar++; // then move it past the space to the next move/command/end
        }
    }
    printBoard(board);
}

void uciLoop(Board& board, SearchInfo *info) {
    // turn off any kind of buffering - stop bad commands etc.
    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);

    char line[INPUT_BUFFER];
    printf("id name %s\n", NAME);
    printf("id author Tim Russell\n");
    printf("uciok\n");

    int MB = 64;

    while (true) {
        memset(&line[0], 0, sizeof(line));
        fflush(stdout);

        if (!fgets(line, INPUT_BUFFER, stdin)) {
            continue;
        }

        if (line[0] == '\n') {
            continue;
        }

        // if 'isready' then output 'readyok'
        if (!strncmp(line, "isready", 7)) {
            printf("readyok\n");
            continue;

        // if 'position' then parse the position
        } else if (!strncmp(line, "position", 8)) {
            parsePosition(line, board);

        // if 'ucinewgame' then clear hash table and parse the start position
        } else if (!strncmp(line, "ucinewgame", 10)) {
            clearHashTable(hashTable);
            parsePosition("position startpos\n", board);

        // if 'go' then parse go!
        } else if (!strncmp(line, "go", 2)) {
             parseGo(line, info, board);

        // extra convenience function: start infinite analysis
        } else if (!strncmp(line, "run", 3)) {
            parseFen(START_FEN, board);
            parseGo("go infinite", info, board);

        // if 'quit' then set quit to true
        } else if (!strncmp(line, "quit", 4)) {
            info->quit = true;
            break;

        // if 'uci' then print info and 'uciok'
        } else if (!strncmp(line, "uci", 3)) {
            printf("id name %s\n", NAME);
            printf("id author Tim Russell\n");
            printf("uciok\n");

        } else if (!strncmp(line, "debug", 4)) {
            debugAnalysisTest(board, info, hashTable);
            break;

        } else if (!strncmp(line, "setoption name Hash value ", 26)) {
            sscanf(line, "%*s %*s %*s %*s %d", &MB);
            if (MB < 4) MB = 4;
            if (MB > 2048) MB = 2048;
            printf("Set Hash to %d MB\n", MB);
            initHashTable(hashTable, MB);
        }

        // if quit was set, then quit!
        if (info->quit) {
            break;
        }
    }
}