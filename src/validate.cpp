#include <cstring>
#include "defs.h"

/// Not using bools here, as compiler adds 'not 0 or 1' checking, and ints should be determined to be 8 bit here anyway

int moveListValid(const MoveList *list, const Board& board) {
    if (list->count < 0 || list->count >= MAX_POSITION_MOVES) {
        return false;
    }
    int from = 0;
    int to = 0;
    for (int moveNum = 0; moveNum < list->count; ++moveNum) {
        to = GET_TO(list->moves[moveNum].move);
        from = GET_FROM(list->moves[moveNum].move);
        if (!squareOnBoard(to) || !squareOnBoard(from)) {
            return false;
        }
        if (!pieceValid(board.pieces[from])) {
            printBoard(board);
            return false;
        }
    }
    return true;
}

int squareIs120(const int sq) {
    return (sq >= 0 && sq < 120);
}

int squareOnBoard(const int sq) {
    return filesBoard[sq] == OFFBOARD ? 0 : 1;
}

int sideValid(const int side) {
    return (side == WHITE || side == BLACK) ? 1 : 0;
}

int fileOrRankValid(const int fileOrRank) {
    return (fileOrRank >= 0 && fileOrRank <= 7) ? 1 : 0;
}

int pieceValid(const int piece) {
    return (piece >= wP && piece <= bK) ? 1 : 0;
}

int pieceValidEmpty(const int piece) {
    return (piece >= EMPTY && piece <= bK) ? 1 : 0;
}

int pieceValidEmptyOffboard(const int pce) {
    return (pieceValidEmpty(pce) || pce == OFFBOARD);
}

void debugAnalysisTest(Board& board, SearchInfo *info, HashTable *table) {
    FILE *file;
    file = fopen("lct2.epd","r");
    char lineIn [1024];

    info->depth = MAX_DEPTH;
    info->timeSet = true;
    int time = 1140000;

    if (file == nullptr) {
        printf("File Not Found\n");
        return;
    } else {
        while(fgets (lineIn , 1024 , file) != nullptr) {
            info->startTime = getTimeMS();
            info->stopTime = info->startTime + time;
            clearHashTable(table);
            parseFen(lineIn, board);
            printf("\n%s\n", lineIn);
            printf("time:%d start:%lld stop:%lld depth:%d timeset:%d\n",
                   time, info->startTime, info->stopTime, info->depth, info->timeSet);
            searchPosition(board, info, table);
            memset(&lineIn[0], 0, sizeof(lineIn));
        }
    }
}

void mirrorEvalTest(Board& board) {
    FILE *file;
    file = fopen("mirror.epd","r");
    char lineIn [1024];
    int ev1 = 0; int ev2 = 0;
    int positions = 0;
    if (file == nullptr) {
        printf("File Not Found\n");
        return;
    } else {
        while (fgets (lineIn , 1024 , file) != nullptr) {
            parseFen(lineIn, board);
            positions++;
            ev1 = evalPosition(board);
            mirrorBoard(board);
            ev2 = evalPosition(board);

            if (ev1 != ev2) {
                printf("\n\n\n");
                parseFen(lineIn, board);
                printBoard(board);
                mirrorBoard(board);
                printBoard(board);
                printf("\n\nMirror Fail:\n%s\n",lineIn);
                getchar();
                return;
            }
            if ((positions % 1000) == 0)   {
                printf("position %d\n",positions);
            }
            memset(&lineIn[0], 0, sizeof(lineIn));
        }
    }
}
