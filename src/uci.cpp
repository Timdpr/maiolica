#include "defs.h"
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

bool debugMode = false; // 'debug on' sends some extra 'info string's

static Board board{}; // the position the gui has set up
static SearchInfo info{};
static std::thread searchThread; // the running search, if any
static int hashMB = 64;
static int moveOverhead = 50; // ms kept back each move, for lag
static TimeMS ponderBudget = 0; // time to use once a ponder search gets 'ponderhit'

/// Print a whole line to the gui at once, so the search & input threads can't interleave
void sendLine(const char *format, ...) {
    static std::mutex mutex;
    char line[4096];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    std::lock_guard lock(mutex);
    std::fputs(line, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

/// Stop any running search & wait for it to send bestmove. Harmless if nothing's running
static void stopSearch() {
    info.stopped = true;
    info.ponder = false;
    if (searchThread.joinable()) {
        searchThread.join();
    }
}

/// How long to think: a share of the clock plus the increment, less the overhead,
/// but never more than 3/4 of what's left. -1 if there's no time limit
static TimeMS timeBudget(long time, long inc, long movesToGo, long moveTime) {
    if (moveTime >= 0) {
        return std::max(1L, moveTime - moveOverhead);
    }
    if (time < 0) {
        return -1;
    }
    long cap = std::max(1L, (time - moveOverhead) * 3 / 4);
    return std::clamp(time / (movesToGo > 0 ? movesToGo : 30) + inc - moveOverhead, 1L, cap); // 30 moves to go if not told
}

/// e.g. go depth 6 wtime 180000 btime 100000 binc 1000 winc 1000 movetime 1000 movestogo 40
static void parseGo(std::istringstream& in) {
    stopSearch();
    long time[2] = {-1, -1}, inc[2] = {0, 0}, movesToGo = 0, moveTime = -1, depth = MAX_DEPTH;
    bool ponder = false;
    info.nodeLimit = 0;
    info.infinite = false;

    std::string token;
    while (in >> token) {
        if (token == "wtime") in >> time[WHITE];
        else if (token == "btime") in >> time[BLACK];
        else if (token == "winc") in >> inc[WHITE];
        else if (token == "binc") in >> inc[BLACK];
        else if (token == "movestogo") in >> movesToGo;
        else if (token == "movetime") in >> moveTime;
        else if (token == "depth") in >> depth;
        else if (token == "nodes") in >> info.nodeLimit;
        else if (token == "infinite") info.infinite = true;
        else if (token == "ponder") ponder = true;
        else if (token == "perft") { // 'go perft N' is just perft
            in >> depth;
            perftTest(int(std::clamp(depth, 1L, long(MAX_DEPTH))), board);
            return;
        }
    }

    info.depth = int(std::clamp(depth, 1L, long(MAX_DEPTH - 2))); // check extensions add one, & the pv tables are MAX_DEPTH long
    info.startTime = getTimeMS();
    TimeMS budget = info.infinite ? -1 : timeBudget(time[board.side], inc[board.side], movesToGo, moveTime);
    ponderBudget = ponder ? budget : 0;
    info.stopTime = info.startTime + budget;
    info.timeSet = budget > 0 && !ponder; // a ponder search starts the clock on 'ponderhit'
    info.ponder = ponder;
    info.stopped = false; // reset here, not in the search thread, so an early 'stop' can't get lost
    if (debugMode) {
        sendLine("info string time budget %lld ms, depth %d", (long long)budget, info.depth);
    }

    // search a copy of the board (on the heap, it's big), so the gui can carry on setting positions
    searchThread = std::thread([copy = std::make_unique<Board>(board)] { searchPosition(*copy, &info, hashTable); });
}

/// The opponent played the move we were pondering on, so start the clock for real
static void ponderHit() {
    if (ponderBudget > 0) {
        info.stopTime = getTimeMS() + ponderBudget;
        info.timeSet = true;
    }
    info.ponder = false;
}

/// position [startpos | fen <fen>] [moves <move> ...]
static void parsePosition(std::istringstream& in) {
    std::string token, fen;
    in >> token;
    if (token == "startpos") {
        fen = START_FEN;
        in >> token; // skip 'moves', if it's there
    } else if (token == "fen") {
        while (in >> token && token != "moves") {
            fen += token + " ";
        }
    } else {
        return;
    }

    Board parsed{};
    if (parseFen(fen.c_str(), parsed) != 0) { // fall back to the start position, rather than search nonsense
        sendLine("info string invalid fen: %s", fen.c_str());
        parseFen(START_FEN, board);
        return;
    }
    while (in >> token) { // play the moves, stopping at the first bad one
        if (parsed.historyPly >= MAX_GAME_MOVES - MAX_DEPTH - 1) { // leave room in history for the search
            sendLine("info string game too long, ignoring moves from %s", token.c_str());
            break;
        }
        int move = token.size() >= 4 ? parseMove(token.c_str(), parsed) : NO_MOVE;
        if (move == NO_MOVE || !makeMove(parsed, move)) {
            sendLine("info string illegal move: %s", token.c_str());
            break;
        }
        parsed.ply = 0; // set ply back to 0, since it gets incremented to 1 in makeMove
    }
    board = parsed;
}

/// setoption name <name> [value <value>] (names aren't case sensitive)
static void setOption(std::istringstream& in) {
    std::string token, name, value;
    in >> token; // 'name'
    while (in >> token && token != "value") {
        name += (name.empty() ? "" : " ") + token;
    }
    std::getline(in >> std::ws, value);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });

    if (name == "hash") {
        hashMB = std::clamp(std::atoi(value.c_str()), 1, 2048);
        initHashTable(hashTable, hashMB);
    } else if (name == "clear hash") {
        clearHashTable(hashTable);
    } else if (name == "move overhead") {
        moveOverhead = std::clamp(std::atoi(value.c_str()), 0, 5000);
    }
    // 'threads' & 'ponder' are accepted, but there's nothing to do
}

static void sendId() {
    sendLine("id name %s", NAME);
    sendLine("id author Tim Russell");
    sendLine("option name Hash type spin default 64 min 1 max 2048");
    sendLine("option name Clear Hash type button");
    sendLine("option name Threads type spin default 1 min 1 max 1");
    sendLine("option name Ponder type check default false");
    sendLine("option name Move Overhead type spin default 50 min 0 max 5000");
    sendLine("uciok");
}

/// Read commands from the gui until 'quit', or until the input closes
void uciLoop() {
    parseFen(START_FEN, board);
    std::string line, command;

    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        command.clear();
        in >> command;

        if (command == "uci") sendId();
        else if (command == "isready") sendLine("readyok"); // if 'isready' then output 'readyok', even mid search
        else if (command == "position") parsePosition(in); // the search has its own copy, so no need to stop it
        else if (command == "go") parseGo(in);
        else if (command == "stop") stopSearch();
        else if (command == "ponderhit") ponderHit();
        else if (command == "ucinewgame") {
            stopSearch();
            clearHashTable(hashTable);
            parseFen(START_FEN, board);
        } else if (command == "setoption") {
            stopSearch(); // the hash table may get swapped out from under it
            setOption(in);
        } else if (command == "debug") {
            in >> command;
            debugMode = command == "on";
        } else if (command == "quit") break;

        // developer commands
        else if (command == "bench") {
            stopSearch();
            bench();
            initHashTable(hashTable, hashMB); // bench uses its own 16MB
        } else if (command == "perft") {
            stopSearch();
            int depth = 1;
            in >> depth;
            perftTest(std::clamp(depth, 1, int(MAX_DEPTH)), board);
        } else if (command == "d") printBoard(board);
        else if (command == "eval") sendLine("info string eval %d cp (side to move)", evalPosition(board));
        else if (command == "run") { // start infinite analysis from the start position
            parseFen(START_FEN, board);
            std::istringstream infinite("infinite");
            parseGo(infinite);
        } else if (command == "epd") { // analyse each position of an epd file, e.g. 'epd lct2.epd'
            stopSearch();
            std::string file = "lct2.epd";
            in >> file;
            debugAnalysisTest(file.c_str(), board, &info, hashTable);
        } else if (!command.empty() && debugMode) {
            sendLine("info string unknown command: %s", command.c_str());
        }
    }
    stopSearch();
}
