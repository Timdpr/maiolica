#include <cstdio>
#include "defs.h"

int main() {

    initAll();

    Board board[1];

    const char* FEN4 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

    parseFen(FEN4, board);
    printBoard(board);
    ASSERT(checkBoard(board));

    int move = 0;
    int from = A2;
    int to = H7;
    int cap = wR;
    int prom = bB;

    move = ( from ) | (to << 7) | ( cap << 14) | (prom << 20 );

    printf("from: %d, to: %d, cap: %d, prom: %d\n",
            GET_FROM(move), GET_TO(move), GET_CAPTURED(move), GET_PROMOTED(move));

    printf("Algebraic from: %s\n", printSquare(from));
    printf("Algebraic to: %s\n", printSquare(to));
    printf("Algebraic move: %s\n", printMove(move));

    return 0;
}
