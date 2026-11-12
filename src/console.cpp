// Terminal play & replay: a pretty board, a game against the engine, and pgn replays.
// Kept apart from the uci code, & nothing here runs during a uci session.
#include "defs.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

static bool colour = false; // ansi colours & unicode pieces, if we're drawing to a terminal
static bool flipped = false; // draw with black at the bottom
static int searchingSide = WHITE; // so scores can be shown from white's point of view
static std::string engineMove; // picked out of the search's 'bestmove' line

/// Draw the board (& who's to move), highlighting the squares of the last move
static void drawBoard(const Board& board, const int lastMove) {
    static const char *glyphs[13] = {" ", "♟", "♞", "♝", "♜", "♛", "♚", "♟", "♞", "♝", "♜", "♛", "♚"};
    for (int row = 0; row < 8; ++row) {
        int rank = flipped ? RANK_1 + row : RANK_8 - row;
        std::printf(" %c ", '1' + rank);
        for (int col = 0; col < 8; ++col) {
            int file = flipped ? FILE_H - col : FILE_A + col;
            int square = FILE_RANK_TO_SQUARE_INDEX(file, rank);
            int piece = board.pieces[square];
            if (colour) { // 256 colour backgrounds, olive-ish where the last move went from & to
                bool light = (file + rank) % 2;
                bool moved = lastMove != NO_MOVE && (square == GET_FROM(lastMove) || square == GET_TO(lastMove));
                int background = moved ? (light ? 186 : 143) : (light ? 180 : 137);
                int foreground = pieceColours[piece] == WHITE ? 231 : 16;
                std::printf("\033[48;5;%d;38;5;%dm %s \033[0m", background, foreground, glyphs[piece]);
            } else {
                std::printf(" %c ", pieceChars[piece]);
            }
        }
        std::printf("\n");
    }
    std::printf("   ");
    for (int col = 0; col < 8; ++col) {
        std::printf(" %c ", 'a' + (flipped ? FILE_H - col : FILE_A + col));
    }
    std::printf("\n");
}

/// Shows the search's output our way: one updating line of thinking, & keeps hold of the bestmove
static void onSearchLine(const char *line) {
    if (!std::strncmp(line, "bestmove ", 9)) {
        engineMove.assign(line + 9, std::strcspn(line + 9, " "));
        return;
    }
    if (std::strncmp(line, "info depth ", 11)) return; // ignore any 'info string's

    std::istringstream in(line);
    std::string token, score, pv;
    long depth = 0, nodes = 0, time = 0;
    while (in >> token) {
        if (token == "depth") in >> depth;
        else if (token == "nodes") in >> nodes;
        else if (token == "time") in >> time;
        else if (token == "score") {
            int value = 0;
            in >> token >> value;
            if (searchingSide == BLACK) value = -value; // white's point of view
            char buf[16];
            if (token == "mate") std::snprintf(buf, sizeof(buf), "%s#%d", value < 0 ? "-" : "", std::abs(value));
            else std::snprintf(buf, sizeof(buf), "%+.2f", value / 100.0);
            score = buf;
        } else if (token == "pv") {
            std::getline(in, pv);
            if (pv.size() > 45) pv = pv.substr(0, pv.rfind(' ', 45)) + " ...";
            break;
        }
    }
    if (colour) std::fputs("\r\033[K", stdout); // overwrite the last line
    std::printf("  depth %2ld  %6s  %6.2fM nodes  %5.1fs  %s", depth, score.c_str(), nodes / 1e6, time / 1000.0, pv.c_str());
    if (!colour) std::printf("\n");
    std::fflush(stdout);
}

/// Does the san (e.g. Nf3, exd5, O-O, e8=Q, R1a3) describe this move?
static bool sanMatches(std::string san, const int move, const Board& board) {
    int from = GET_FROM(move), to = GET_TO(move), piece = board.pieces[from], promoted = GET_PROMOTED(move);
    if (move & MFLAG_CASTLING) {
        return (filesBoard[to] == FILE_G && (san == "O-O" || san == "0-0"))
            || (filesBoard[to] == FILE_C && (san == "O-O-O" || san == "0-0-0"));
    }
    // promotion piece, as '=Q' or just 'Q' on the end
    char promotion = 0;
    if (san.size() > 2 && std::strchr("QRBN", san.back()) && std::islower(san[0])) {
        promotion = san.back();
        san.pop_back();
        if (san.back() == '=') san.pop_back();
    }
    if (promotion != (promoted ? std::toupper(pieceChars[promoted]) : 0)) return false;
    // destination square is the last two characters
    if (san.size() < 2 || san[san.size() - 2] != 'a' + filesBoard[to] || san.back() != '1' + ranksBoard[to]) return false;
    san.resize(san.size() - 2);
    // then the piece letter (none for pawns)
    char type = 'P';
    if (!san.empty() && std::isupper(san[0])) {
        type = san[0];
        san.erase(0, 1);
    }
    if (std::toupper(pieceChars[piece]) != type) return false;
    // anything left is a capture 'x' (or the '-' of e2-e4) or the from file/rank
    for (char c : san) {
        if (!std::strchr("abcdefgh12345678x-", c)) return false; // so a mistyped 'nf3' isn't read as a pawn to f3
        if (c >= 'a' && c <= 'h' && c - 'a' != filesBoard[from]) return false;
        if (c >= '1' && c <= '8' && c - '1' != ranksBoard[from]) return false;
    }
    return true;
}

/// A legal move from uci (e2e4, e7e8q) or san (Nf3, exd5+, O-O) notation. NO_MOVE if it's illegal or ambiguous
static int parseAnyMove(const std::string& text, Board& board) {
    if (text.size() == 4 || text.size() == 5) {
        int move = parseMove(text.c_str(), board);
        if (move != NO_MOVE && makeMove(board, move)) {
            undoMove(board);
            return move;
        }
    }
    std::string san;
    for (char c : text) {
        if (!std::strchr("+#!?", c)) san += c;
    }
    MoveList moveList[1];
    generateAllMoves(board, moveList);
    int found = NO_MOVE;
    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        int move = moveList->moves[moveNum].move;
        if (!sanMatches(san, move, board) || !makeMove(board, move)) continue;
        undoMove(board);
        if (found != NO_MOVE) return NO_MOVE; // ambiguous
        found = move;
    }
    return found;
}

/// Play a move for real, keeping room in history for the search
static bool playMove(Board& board, const int move) {
    if (move == NO_MOVE || board.historyPly >= MAX_GAME_MOVES - MAX_DEPTH - 1 || !makeMove(board, move)) return false;
    board.ply = 0;
    return true;
}

/// The result if the game's over (mate, stalemate, fifty moves, threefold, bare minor pieces), otherwise nullptr
static const char *gameResult(Board& board) {
    MoveList moveList[1];
    generateAllMoves(board, moveList);
    bool canMove = false;
    for (int moveNum = 0; moveNum < moveList->count && !canMove; ++moveNum) {
        if (makeMove(board, moveList->moves[moveNum].move)) {
            undoMove(board);
            canMove = true;
        }
    }
    if (!canMove) {
        if (!isSquareAttacked(board.kingSq[board.side], board.side ^ 1, board)) return "1/2-1/2 stalemate";
        return board.side == WHITE ? "0-1 black mates" : "1-0 white mates";
    }
    if (board.fiftyMove >= 100) return "1/2-1/2 fifty move rule";
    int repeats = 0;
    for (int i = std::max(0, board.historyPly - board.fiftyMove); i < board.historyPly; ++i) {
        repeats += board.history[i].positionKey == board.positionKey;
    }
    if (repeats >= 2) return "1/2-1/2 threefold repetition";
    const int *count = board.pieceCounts;
    if (!count[wP] && !count[bP] && !count[wR] && !count[bR] && !count[wQ] && !count[bQ]
        && board.minorPieces[WHITE] + board.minorPieces[BLACK] <= 1) return "1/2-1/2 insufficient material";
    return nullptr;
}

static int lastMove(const Board& board) {
    return board.historyPly > 0 ? board.history[board.historyPly - 1].move : NO_MOVE;
}

/// Search the position with the console's limits & return the move it picks
static int think(Board& board, const int moveTime, const int depth) {
    static SearchInfo info{};
    info.depth = depth;
    info.nodeLimit = 0;
    info.infinite = false;
    info.ponder = false;
    info.stopped = false;
    info.startTime = getTimeMS();
    info.stopTime = info.startTime + moveTime;
    info.timeSet = moveTime > 0;
    searchingSide = board.side;
    searchPosition(board, &info, hashTable); // on this thread: there's nothing else to listen to
    std::printf("\n");
    return parseAnyMove(engineMove, board);
}

static void help() {
    std::printf("Enter moves as e2e4 or Nf3. Commands:\n"
                "  new            new game, you're white\n"
                "  go             engine plays the side to move (so 'new' then 'go' to play black)\n"
                "  force          engine stops playing; enter moves for both sides\n"
                "  auto           engine plays both sides until the game ends\n"
                "  undo           take back a move (or two, against the engine)\n"
                "  flip           turn the board round\n"
                "  time <s>       engine's thinking time per move (0 for no limit)\n"
                "  depth <n>      engine's depth limit (0 for no limit)\n"
                "  fen <fen>      set up a position\n"
                "  board          show the board\n"
                "  quit\n");
}

/// A game against the engine (or the engine against itself) in the terminal
void consoleLoop() {
    colour = isatty(STDOUT_FILENO) && !std::getenv("NO_COLOR");
    lineHandler = onSearchLine;
    static Board board{};
    parseFen(START_FEN, board);
    enum { NOBODY = 3 };
    int engineSide = BLACK; // WHITE, BLACK, BOTH or NOBODY
    int moveTime = 3000, depth = MAX_DEPTH - 2;

    std::printf("%s console. You're white: enter moves like e2e4 or Nf3, or 'help'\n\n", NAME);
    drawBoard(board, NO_MOVE);
    std::string line, command;
    while (true) {
        const char *result = gameResult(board);
        if (result) {
            std::printf("\n%s\n", result);
            if (engineSide == BOTH) engineSide = NOBODY; // stop an auto game, so you can look around
        }
        if (!result && (engineSide == board.side || engineSide == BOTH)) {
            int move = think(board, moveTime, depth);
            if (!playMove(board, move)) {
                std::printf("engine couldn't move\n");
                engineSide = NOBODY;
                continue;
            }
            if (colour && engineSide == BOTH) std::printf("\033[H\033[2J"); // animate a self play game
            std::printf("\n%s plays %s\n\n", board.side == WHITE ? "Black" : "White", printMove(move));
            drawBoard(board, move);
            continue;
        }

        std::printf("\n%s> ", board.side == WHITE ? "white" : "black");
        std::fflush(stdout);
        if (!std::getline(std::cin, line)) break;
        std::istringstream in(line);
        command.clear();
        in >> command;

        if (command.empty()) continue;
        else if (command == "quit" || command == "exit") break;
        else if (command == "help") help();
        else if (command == "new") {
            parseFen(START_FEN, board);
            clearHashTable(hashTable);
            engineSide = BLACK;
            drawBoard(board, NO_MOVE);
        } else if (command == "go") engineSide = board.side;
        else if (command == "force") engineSide = NOBODY;
        else if (command == "auto") engineSide = BOTH;
        else if (command == "undo") {
            int takeBacks = engineSide == WHITE || engineSide == BLACK ? 2 : 1; // back to your move
            for (int i = 0; i < takeBacks && board.historyPly > 0; ++i) undoMove(board);
            board.ply = 0;
            drawBoard(board, lastMove(board));
        } else if (command == "flip") {
            flipped = !flipped;
            drawBoard(board, lastMove(board));
        } else if (command == "board") drawBoard(board, lastMove(board));
        else if (command == "time") {
            double seconds = 3;
            in >> seconds;
            moveTime = int(seconds * 1000);
        } else if (command == "depth") {
            in >> depth;
            depth = depth <= 0 ? MAX_DEPTH - 2 : std::min(depth, MAX_DEPTH - 2);
        } else if (command == "fen") {
            std::string fen;
            std::getline(in >> std::ws, fen);
            Board parsed{};
            if (parseFen(fen.c_str(), parsed) != 0) std::printf("that fen doesn't look right\n");
            else {
                board = parsed;
                engineSide = NOBODY;
                drawBoard(board, NO_MOVE);
            }
        } else if (playMove(board, parseAnyMove(command, board))) {
            drawBoard(board, lastMove(board));
        } else {
            std::printf("not a legal move or command: %s ('help' for a list)\n", command.c_str());
        }
    }
    lineHandler = nullptr;
}

/// Play through every game in a pgn file on the board, waiting 'delay' ms between moves
void replayPgn(const char *file, const int delay) {
    colour = isatty(STDOUT_FILENO) && !std::getenv("NO_COLOR");
    std::ifstream stream(file);
    if (!stream) {
        std::printf("can't open %s\n", file);
        return;
    }
    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    static Board board{};
    std::string white, black, termination;
    int moves = 0;
    auto newGame = [&] { // a clean slate, for the next game's tags to fill in
        parseFen(START_FEN, board);
        white = black = "?";
        termination.clear();
        moves = 0;
    };
    newGame();

    for (size_t i = 0; i < text.size(); ) {
        char c = text[i];
        if (std::isspace((unsigned char)c)) { ++i; continue; }
        if (c == '{' || c == ';') { // skip comments
            i = text.find(c == '{' ? '}' : '\n', i);
            i = i == std::string::npos ? text.size() : i + 1;
            continue;
        }
        if (c == '(') { // and variations, which can nest
            for (int nesting = 0; i < text.size(); ++i) {
                nesting += text[i] == '(' ? 1 : text[i] == ')' ? -1 : 0;
                if (!nesting) break;
            }
            ++i;
            continue;
        }
        if (c == '[') { // a tag pair: [Name "value"]
            if (moves) newGame(); // tags after moves: the last game had no result on the end
            size_t end = text.find(']', i);
            std::string tag = text.substr(i + 1, end - i - 1);
            i = end == std::string::npos ? text.size() : end + 1;
            size_t quote = tag.find('"');
            if (quote == std::string::npos) continue;
            std::string name = tag.substr(0, tag.find(' ')), value = tag.substr(quote + 1, tag.rfind('"') - quote - 1);
            if (name == "White") white = value;
            else if (name == "Black") black = value;
            else if (name == "Termination") termination = value;
            else if (name == "FEN" && parseFen(value.c_str(), board) != 0) parseFen(START_FEN, board);
            continue;
        }

        size_t end = text.find_first_of(" \t\r\n{(;", i);
        std::string token = text.substr(i, end - i);
        i = end == std::string::npos ? text.size() : end;
        if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") { // end of a game
            std::printf("\n%s %s\n\n", token.c_str(), termination.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(delay * 4));
            newGame();
            continue;
        }
        size_t dot = token.rfind('.');
        if (dot != std::string::npos && std::isdigit((unsigned char)token[0])) token.erase(0, dot + 1); // move numbers, like '12.' or '12...e5'
        if (token.empty() || token[0] == '$') continue; // nothing left, or an annotation code like $1

        int move = parseAnyMove(token, board);
        if (!playMove(board, move)) {
            std::printf("couldn't read move '%s', skipping the rest of this game\n", token.c_str());
            i = text.find_first_of("[", i); // on to the next game's tags
            newGame();
            continue;
        }
        moves++;
        if (colour) std::printf("\033[H\033[2J"); // redraw in place
        std::printf("%s vs %s, move %d: %s\n\n", white.c_str(), black.c_str(), (moves + 1) / 2, token.c_str());
        drawBoard(board, move);
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
}
