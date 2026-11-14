// Microbenchmarks of the hot functions, for quick iteration (bench & perft still have the final say,
// since LTO inlines these into the search & a warm loop flatters them). Build with the microbench preset:
//   cmake --preset microbench && cmake --build --preset microbench && cmake-build-microbench/microbench
#include "defs.h"
#include <benchmark/benchmark.h>
#include <vector>

/// A spread of opening, middlegame & endgame positions
static const char *fens[] = {
    START_FEN,
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "2r3k1/pp3ppp/4p3/3pP3/3P4/P4N2/1P3PPP/2R3K1 b - - 0 25",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1",
};

/// Parse each fen once, into heap boards (they're big)
static std::vector<Board>& boards() {
    static std::vector<Board> list = [] {
        initAll();
        std::vector<Board> b(std::size(fens));
        for (size_t i = 0; i < b.size(); ++i) parseFen(fens[i], b[i]);
        return b;
    }();
    return list;
}

static void generateMoves(benchmark::State& state) {
    MoveList list;
    for (auto _ : state)
        for (auto& board : boards()) { generateAllMoves(board, list); benchmark::DoNotOptimize(list); }
}
BENCHMARK(generateMoves);

static void generateCaptures(benchmark::State& state) {
    MoveList list;
    for (auto _ : state)
        for (auto& board : boards()) { generateAllCaptureMoves(board, list); benchmark::DoNotOptimize(list); }
}
BENCHMARK(generateCaptures);

/// Make & undo every pseudo-legal move (illegal ones get undone inside makeMove)
static void makeUndo(benchmark::State& state) {
    MoveList list;
    for (auto _ : state)
        for (auto& board : boards()) {
            generateAllMoves(board, list);
            for (int i = 0; i < list.count; ++i)
                if (makeMove(board, list.moves[i].move)) undoMove(board);
        }
}
BENCHMARK(makeUndo);

/// Is each side's king in check? (makeMove makes the same call for king moves, en passant & moves out of check)
static void squareAttacked(benchmark::State& state) {
    for (auto _ : state)
        for (auto& board : boards()) {
            benchmark::DoNotOptimize(isSquareAttacked(board.kingSq[WHITE], BLACK, board));
            benchmark::DoNotOptimize(isSquareAttacked(board.kingSq[BLACK], WHITE, board));
        }
}
BENCHMARK(squareAttacked);

static void evaluate(benchmark::State& state) {
    for (auto _ : state)
        for (auto& board : boards()) benchmark::DoNotOptimize(evalPosition(board));
}
BENCHMARK(evaluate);

BENCHMARK_MAIN();
