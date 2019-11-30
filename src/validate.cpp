#include <cstring>
#include "defs.h"

/// Not using bools here, as compiler adds 'not 0 or 1' checking, and ints should be determined to be 8 bit here anyway

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

int pieceValidOrEmpty(const int piece) {
    return (piece >= EMPTY && piece <= bK) ? 1 : 0;
}

void mirrorEvalTest(Board *board) {
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