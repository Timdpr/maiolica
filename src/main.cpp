#include <cstdio>
#include <iostream>
#include "defs.h"

int main() {

    const char *PAWNMOVESW = "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1";
    const char *PAWNMOVESB = "rnbqkbnr/p1p1p3/3p3p/1p1p4/2P1Pp2/8/PP1P1PpP/RNBQKB1R b KQkq e3 0 1";
    const char *KNIGHTSKINGS = "5k2/1n6/4n3/6N1/8/3N4/8/5K2 w - - 0 1";
    const char *ROOKS = "6k1/8/5r2/8/1nR5/5N2/8/6K1 b - - 0 1";
    const char *QUEENS = "6k1/8/4nq2/8/1nQ5/5N2/1N6/6K1 b - - 0 1 ";
    const char *BISHOPS = "6k1/1b6/4n3/8/1n4B1/1B3N2/1N6/2b3K1 b - - 0 1 ";
    const char *CASTLE1 = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
    const char *CASTLE2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    const char *TRICKYFEN = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    const char *TRICKYFEN2 = "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N w - - 0 1";

    initAll();

    Board board[1];
    MoveList moveList[1];

    parseFen(TRICKYFEN2, board);
//    perftTest(5, board);

    char input[6];
    printBoard(board);
    while (true) {
        printf("\nPlease enter a move, 't' to undo last move, or 'q' to quit: ");
        fgets(input, 7, stdin);
        if (input[4] == '\n') input[4] = '\0';
        if (input[5] == '\n') input[5] = '\0';

        if (input[0] == 'q') {
            break;
        } else if (input[0] == 't') {
            takeMove(board);
            printBoard(board);
        } else {
            int move = parseMove(input, board);
            if (move != NO_MOVE) {
                makeMove(board, move);
                printBoard(board);
            } else {
                printf("- Move not recognised! -\n");
            }
        }

        fflush(stdin);
    }

    return 0;
}
