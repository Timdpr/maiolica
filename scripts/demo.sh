#!/usr/bin/env bash
# One game for show: maiolica against itself (or any uci engine), then replayed on the terminal board.
#   scripts/demo.sh [opponent=maiolica] [tc=10+0.1] [ms per move=600]
set -euo pipefail
cd "$(dirname "$0")/.."

ENGINE=$PWD/cmake-build-release/maiolica
OPPONENT=${1:-$ENGINE}
TC=${2:-10+0.1}
DELAY=${3:-600}
FASTCHESS=${FASTCHESS:-external/fastchess/fastchess}
BOOK=${BOOK:-books/8moves_v3.pgn}

cmake --preset release >/dev/null && cmake --build --preset release >/dev/null
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

# a random book opening if we have the book, otherwise every self play game would be the same
OPENINGS=()
[[ -f $BOOK ]] && OPENINGS=(-openings file="$BOOK" format="${BOOK##*.}" order=random)

echo "playing a $TC game..."
"$FASTCHESS" -engine cmd="$ENGINE" name=Maiolica -engine cmd="$OPPONENT" name="$(basename "$OPPONENT")" \
    -each tc="$TC" -rounds 1 -games 1 ${OPENINGS[@]+"${OPENINGS[@]}"} \
    -pgnout file="$OUT/game.pgn" -config outname="$OUT/config.json" >/dev/null
"$ENGINE" replay "$OUT/game.pgn" "$DELAY"
