#include "defs.h"

const char pieceChars[] = ".PNBRQKpnbrqk";
const char sideChars[]  = "wb-";
const char rankChars[]  = "12345678";
const char fileChars[]  = "ABCDEFGH";

const int bigPieces[13]    = { FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE };
const int majorPieces[13]  = { FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE };
const int minorPieces[13]  = { FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE };
const int piecePawn[13]    = { FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE };
const int pieceValues[13]  = { 0, 100, 325, 325, 550, 1000, 50000, 100, 325, 325, 550, 1000, 50000 };
const int pieceColours[13] = { BOTH, WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK };

const int pieceKnight[13]      = { FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE };
const int pieceBishopQueen[13] = { FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE };
const int pieceRookQueen[13]   = { FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE };
const int pieceKing[13]        = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE };
const int pieceSlides[13]      = { FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE };

/// Use to get square index for black, as above tables are designed for white
int mirror64[64] = {
        56	,	57	,	58	,	59	,	60	,	61	,	62	,	63	,
        48	,	49	,	50	,	51	,	52	,	53	,	54	,	55	,
        40	,	41	,	42	,	43	,	44	,	45	,	46	,	47	,
        32	,	33	,	34	,	35	,	36	,	37	,	38	,	39	,
        24	,	25	,	26	,	27	,	28	,	29	,	30	,	31	,
        16	,	17	,	18	,	19	,	20	,	21	,	22	,	23	,
        8	,	9	,	10	,	11	,	12	,	13	,	14	,	15	,
        0	,	1	,	2	,	3	,	4	,	5	,	6	,	7
};