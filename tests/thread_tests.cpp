// The search with helper threads. With more than one thread a search isn't repeatable (which thread writes a table
// entry first is down to timing), so the bench signature has nothing to say about it. This checks what must be true
// however the threads fall: a forced mate is found, the move played is legal & is the last line's first move, the
// node count adds up, and the search comes back when a limit is reached or it is told to stop.
// In a debug or sanitiser build every ASSERT runs in every thread. Usage: thread_tests
#include "defs.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static const struct { const char *name, *fen; int depth; const char *move, *score; } cases[] = {
    {"mate-in-1", "6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", 4, "a1a8", "mate 1"},
    {"mate-in-2", "r2qkb1r/pp2nppp/3p4/2pNN1B1/2BnP3/3P4/PPP2PPP/R2bK2R w KQkq - 1 1", 5, "d5f6", "mate 2"},
    {"startpos", START_FEN, 6, nullptr, nullptr},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, nullptr, nullptr},
    {"rook-ending", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, nullptr, nullptr},
};

// what the search last said: only its main thread prints, so these are written by one thread at a time
static char bestMove[16]; // the move after 'bestmove'
static char lastScore[16]; // the score of the last depth printed, as uci has it: 'cp 25', 'mate 2'
static char lastPvMove[16]; // & the first move of its line

/// Copy the word at 'from' (two words, for a score), without the rest of the line
static void copyWords(char *to, size_t size, const char *from, int words) {
    size_t length = 0;
    while (from[length] && length + 1 < size && (from[length] != ' ' || --words > 0)) {
        to[length] = from[length];
        ++length;
    }
    to[length] = '\0';
}

static void readLine(const char *line) {
    if (!std::strncmp(line, "bestmove ", 9)) {
        copyWords(bestMove, sizeof(bestMove), line + 9, 1);
    } else if (!std::strncmp(line, "info depth ", 11)) {
        const char *score = std::strstr(line, " score ");
        const char *pv = std::strstr(line, " pv ");
        if (score) copyWords(lastScore, sizeof(lastScore), score + 7, 2);
        copyWords(lastPvMove, sizeof(lastPvMove), pv ? pv + 4 : "", 1);
    }
}

static int failures = 0;
static void expect(bool ok, const char *name, int threads, const char *what) {
    if (!ok) {
        std::printf("FAIL %s, %d threads: %s (bestmove %s, score %s)\n", name, threads, what, bestMove, lastScore);
        failures++;
    }
}

/// Is the move the search played a legal one in the position it was given?
static bool bestMoveIsLegal(const char *fen) {
    Board board{};
    parseFen(fen, board);
    const int move = parseMove(bestMove, board);
    return move != NO_MOVE && makeMove(board, move);
}

static void newSearch(const char *fen, Board& board, SearchInfo& info, int threads, int depth) {
    bestMove[0] = lastScore[0] = lastPvMove[0] = '\0';
    clearHashTable(hashTable);
    parseFen(fen, board);
    info.threads = threads;
    info.depth = depth;
    info.startTime = getTimeMS();
}

int main() {
    initAll();
    initHashTable(hashTable, 16);
    lineHandler = readLine;

    // to a fixed depth, twice over with each number of threads
    for (const int threads : {1, 2, 4, 8}) {
        long long nodes = 0;
        TimeMS start = getTimeMS();
        for (int repeat = 0; repeat < 2; ++repeat) {
            for (const auto& c : cases) {
                Board board{};
                SearchInfo info{};
                newSearch(c.fen, board, info, threads, c.depth);
                searchPosition(board, info, hashTable);
                nodes += info.nodes;
                expect(bestMoveIsLegal(c.fen), c.name, threads, "the move played isn't legal");
                expect(!std::strcmp(bestMove, lastPvMove), c.name, threads, "the move played isn't the last line's first move");
                expect(!c.move || !std::strcmp(bestMove, c.move), c.name, threads, "the mating move wasn't played");
                expect(!c.score || !std::strcmp(lastScore, c.score), c.name, threads, "the mate wasn't found");
                // the total is every thread's count, the main thread's among them
                expect(threads == 1 ? info.nodes == board.nodes : info.nodes >= board.nodes, c.name, threads, "the node count doesn't add up");
            }
        }
        std::printf("%d thread%s %8lld nodes %5lld ms\n", threads, threads == 1 ? " " : "s", nodes, (long long)(getTimeMS() - start));
    }

    // a node limit is for all the threads together, & the main thread is the one that keeps count
    {
        Board board{};
        SearchInfo info{};
        newSearch(cases[3].fen, board, info, 4, MAX_DEPTH - 2);
        info.nodeLimit = 50000;
        searchPosition(board, info, hashTable);
        expect(info.nodes >= info.nodeLimit && bestMoveIsLegal(cases[3].fen), "node limit", 4, "stopped short, or without a move");
    }

    // 'go infinite': nothing ends it but being told to stop, as the input thread does. The helpers must end with it
    {
        Board board{};
        SearchInfo info{};
        newSearch(START_FEN, board, info, 4, MAX_DEPTH - 2);
        info.infinite = true;
        std::thread search([&] { searchPosition(board, info, hashTable); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        info.stopped = true;
        search.join();
        expect(bestMoveIsLegal(START_FEN), "stop", 4, "stopped without a legal move");
    }

    free(hashTable.hTable);
    std::printf("%d failure(s)\n", failures);
    return failures > 0;
}
