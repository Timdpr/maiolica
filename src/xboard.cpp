#include "defs.h"
#include <cstdio>
#include <cstring>

/// Check for threefold repetition
int threefoldRepetition(const Board *board) {
    int rep = 0;
    for (int i = 0; i < board->historyPly; ++i) {
        if (board->history[i].positionKey == board->positionKey) {
            rep++;
        }
    }
    return rep;
}

// If any of the conditions here are met, then a mate can happen
int drawMaterial(const Board *board) {
    if (board->pieceCounts[wP] || board->pieceCounts[bP]) { return FALSE; }
    if (board->pieceCounts[wQ] || board->pieceCounts[bQ] || board->pieceCounts[wR] || board->pieceCounts[bR]) { return FALSE; }
    if (board->pieceCounts[wB] > 1 || board->pieceCounts[bB] > 1) { return FALSE; }
    if (board->pieceCounts[wN] > 1 || board->pieceCounts[bN] > 1) { return FALSE; }
    if (board->pieceCounts[wN] && board->pieceCounts[wB]) { return FALSE; }
    if (board->pieceCounts[bN] && board->pieceCounts[bB]) { return FALSE; }
    return TRUE;
}

int checkResult(Board *board) {
    if (board->fiftyMove > 100) { // todo: not accounting for side, potentially inaccurate
        printf("1/2-1/2 {50 move rule (claimed by Maiolica)}\n"); return TRUE;
    }
    if (threefoldRepetition(board) >= 2) {
        printf("1/2-1/2 {Threefold repetition (claimed by Maiolica)}\n"); return TRUE;
    }
    if (drawMaterial(board) == TRUE) {
        printf("1/2-1/2 {Insufficient material (claimed by Maiolica)}\n"); return TRUE;
    }

    MoveList moveList[1];
    generateAllMoves(board, moveList);

    int found = 0;
    for (int moveNum = 0; moveNum < moveList->count; ++moveNum) {
        if (!makeMove(board, moveList->moves[moveNum].move)) {
            continue;
        }
        found++;
        takeMove(board);
        break;
    }
    if (found != 0) {
        return FALSE;
    }

    int inCheck = isSquareAttacked(board->kingSq[board->side], board->side^1, board);
    if (inCheck == TRUE) {
        if (board->side == WHITE) {
            printf("0-1 {Black mates (claimed by Maiolica)}\n");
            return TRUE;
        } else {
            printf("0-1 {White mates (claimed by Maiolica)}\n");
            return TRUE;
        }
    } else {
        printf("\n1/2-1/2 {Stalemate (claimed by Maiolica)}\n");
        return TRUE;
    }
}

void printOptions() {
    printf("feature ping=1 setboard=1 colors=0 usermove=1\n");
    printf("feature done=1\n");
}

void xBoardLoop(Board *board, SearchInfo *info) {
    info->gameMode = XBOARD_MODE;
    info->postThinking = TRUE;
    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);
    printOptions();

    int movesToGo[2] = {30, 30};
    int moveTime = -1;
    int inc = 0;
    int timeLeft;
    int sec;
    int mps;
    char inBuf[80], command[80];

    int engineSide = BLACK;
    parseFen(START_FEN, board);
    int depth = -1;
    int time = -1;

    while(TRUE) {

        fflush(stdout);

        if (board->side == engineSide && checkResult(board) == FALSE) {
            info->startTime = getTimeMS();
            info->depth = depth;

            if (time != -1) {
                info->timeSet = TRUE;
                time /= movesToGo[board->side];
                time -= 50;
                info->stopTime = info->startTime + time + inc;
            }

            if (depth == -1 || depth > MAX_DEPTH) {
                info->depth = MAX_DEPTH;
            }

            printf("time:%d start:%lld stop:%lld depth:%d timeset:%d movestogo:%d mps:%d\n",
                   time, info->startTime, info->stopTime, info->depth, info->timeSet, movesToGo[board->side], mps);
            searchPosition(board, info);

            if (mps != 0) {
                movesToGo[board->side^1]--;
                if (movesToGo[board->side^1] < 1) {
                    movesToGo[board->side^1] = mps;
                }
            }
        }

        fflush(stdout);

        memset(&inBuf[0], 0, sizeof(inBuf));
        fflush(stdout);
        if (!fgets(inBuf, 80, stdin)) {
            continue;
        }

        sscanf(inBuf, "%s", command);

        printf("command seen:%s\n", inBuf);

        if (!strcmp(command, "quit")) {
            info->quit = TRUE;
            break;
        }

        if (!strcmp(command, "force")) {
            engineSide = BOTH;
            continue;
        }

        if (!strcmp(command, "protover")) {
            printOptions();
            continue;
        }

        if (!strcmp(command, "sd")) {
            sscanf(inBuf, "sd %d", &depth);
            continue;
        }

        if (!strcmp(command, "st")) {
            sscanf(inBuf, "st %d", &moveTime);
            continue;
        }

        if (!strcmp(command, "time")) {
            sscanf(inBuf, "time %d", &time);
            time *= 10;
            continue;
        }

        if (!strcmp(command, "level")) {
            sec = 0;
            moveTime = -1;
            if (sscanf(inBuf, "level %d %d %d", &mps, &timeLeft, &inc) != 3) {
                sscanf(inBuf, "level %d %d:%d %d", &mps, &timeLeft, &sec, &inc);
            }
            timeLeft *= 60000;
            timeLeft += sec * 1000;
            movesToGo[0] = movesToGo[1] = 30;
            if (mps != 0) {
                movesToGo[0] = movesToGo[1] = mps;
            }
            time = -1;
            continue;
        }

        if (!strcmp(command, "ping")) {
            printf("pong%s\n", inBuf+4);
            continue;
        }

        if (!strcmp(command, "new")) {
            engineSide = BLACK;
            parseFen(START_FEN, board);
            depth = -1;
            time = -1;
            continue;
        }

        if (!strcmp(command, "setboard")) {
            engineSide = BOTH;
            parseFen(inBuf+9, board);
            continue;
        }

        if (!strcmp(command, "go")) {
            engineSide = board->side;
            continue;
        }

        if (!strcmp(command, "usermove")) {
            movesToGo[board->side]--;
            int move = parseMove(inBuf+9, board);
            if (move == NO_MOVE) continue;
            makeMove(board, move);
            board->ply=0;
        }
    }
}

void consoleLoop(Board *board, SearchInfo *info) {

    printf("\nWelcome to Maiolica in console mode!\n");
    printf("Type 'help' for commands\n");

    info->gameMode = CONSOLE_MODE;
    info->postThinking = TRUE;
    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);

    int depth = MAX_DEPTH, movetime = 3000;
    char inBuf[80], command[80];

    int engineSide = BLACK;
    parseFen(START_FEN, board);

    while(TRUE) {

        fflush(stdout);

        if(board->side == engineSide && checkResult(board) == FALSE) {
            info->startTime = getTimeMS();
            info->depth = depth;

            if(movetime != 0) {
                info->timeSet = TRUE;
                info->stopTime = info->startTime + movetime;
            }

            searchPosition(board, info);
        }

        printf("\nMaiolica > ");

        fflush(stdout);

        memset(&inBuf[0], 0, sizeof(inBuf));
        fflush(stdout);
        if (!fgets(inBuf, 80, stdin))
            continue;

        sscanf(inBuf, "%s", command);

        if (!strcmp(command, "help")) {
            printf("\nCommands:\n");
            printf("quit - quit game\n");
            printf("force - computer will not think\n");
            printf("print - show board\n");
            printf("post - show thinking\n");
            printf("nopost - do not show thinking\n");
            printf("new - start new game\n");
            printf("go - set computer thinking\n");
            printf("depth x - set depth to x\n");
            printf("time x - set thinking time to x seconds (depth still applies if set)\n");
            printf("view - show current depth and movetime settings\n");
            printf("\n** note ** - to reset time and depth, set to 0\n");
            printf("enter moves using b7b8q notation\n");
            continue;
        }

        if (!strcmp(command, "quit")) {
            info->quit = TRUE;
            break;
        }

        if (!strcmp(command, "post")) {
            info->postThinking = TRUE;
            continue;
        }

        if (!strcmp(command, "print")) {
            printBoard(board);
            continue;
        }

        if (!strcmp(command, "nopost")) {
            info->postThinking = FALSE;
            continue;
        }

        if (!strcmp(command, "force")) {
            engineSide = BOTH;
            continue;
        }

        if (!strcmp(command, "view")) {
            if (depth == MAX_DEPTH) printf("depth not set ");
            else printf("depth %d",depth);

            if (movetime != 0) printf(" movetime %ds\n", movetime/1000);
            else printf(" movetime not set\n");

            continue;
        }

        if (!strcmp(command, "depth")) {
            sscanf(inBuf, "depth %d", &depth);
            if (depth==0) depth = MAX_DEPTH;
            continue;
        }

        if (!strcmp(command, "time")) {
            sscanf(inBuf, "time %d", &movetime);
            movetime *= 1000;
            continue;
        }

        if (!strcmp(command, "new")) {
            engineSide = BLACK;
            parseFen(START_FEN, board);
            continue;
        }

        if (!strcmp(command, "go")) {
            engineSide = board->side;
            continue;
        }

        int move = parseMove(inBuf, board);
        if (move == NO_MOVE) {
            printf("Command unknown: %s", inBuf);
            continue;
        }
        makeMove(board, move);
        board->ply=0;
    }
}